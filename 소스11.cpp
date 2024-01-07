#include <iostream>
#include <list>
#include<vector>
#include <memory>
using namespace std;

template<typename T>
    requires (alignof(T) == 8)
class AtomicMemoryPool
{
private:
    struct alignas(8) Block
    {
        std::atomic<uint64_t> combined;
    };

    std::byte* blockStart;
    //std::vector<std::byte> memoryBlock;
    std::atomic<uint64_t> head;
    const size_t blockSize;
    const size_t maxBlockCount;
    static constexpr uint32_t maxTagValue = (1 << 19) - 1;
private:
    const uint64_t packPointerAndTag(const Block* const ptr, const uint32_t tag) const noexcept {
        const uintptr_t ptrVal = reinterpret_cast<uintptr_t>(ptr);
        return (static_cast<const uint64_t>(ptrVal) & 0x7FFFFFFFFFF) | (static_cast<const uint64_t>(tag) << 45);
    }

    Block* const unpackPointer(const uint64_t combined) const noexcept {
        return reinterpret_cast<Block* const>(combined & 0x7FFFFFFFFFF);
    }

    const uint32_t unpackTag(const uint64_t combined) const noexcept {
        return static_cast<const uint32_t>(combined >> 45);
    }

    void initialize() noexcept
    {
        //std::byte* const blockStart = memoryBlock.data();

        for (size_t i = 0; i < maxBlockCount - 1; ++i)
        {
            Block* const block = new (blockStart + i * blockSize) Block();
            const uint64_t nextCombined = packPointerAndTag(reinterpret_cast<Block*>(blockStart + (i + 1) * blockSize), 0);
            block->combined.store(nextCombined, std::memory_order_relaxed);
        }

        Block* const lastBlock = new (blockStart + (maxBlockCount - 1) * blockSize) Block();
        lastBlock->combined.store(packPointerAndTag(nullptr, 0), std::memory_order_relaxed);

        head.store(packPointerAndTag(reinterpret_cast<Block* const>(blockStart), 0), std::memory_order_relaxed);
    }
public:
    explicit AtomicMemoryPool(const size_t count)
        : blockSize{ sizeof(Block) + sizeof(T) }
        , maxBlockCount{ count }
    {
        const size_t totalSize = blockSize * maxBlockCount;
        blockStart = static_cast<std::byte*>(operator new[](totalSize, std::align_val_t(std::hardware_constructive_interference_size)));
        //memoryBlock.assign(rawMemory, rawMemory + totalSize);
        initialize();
    }

    ~AtomicMemoryPool()
    {
        operator delete[](blockStart, std::align_val_t(std::hardware_constructive_interference_size));
    }

    T* const allocate() noexcept
    {
        uint64_t oldCombined = head.load(std::memory_order_relaxed);
        uint64_t newCombined;
        Block* currentBlock;
        do {
            currentBlock = unpackPointer(oldCombined);
            if (!currentBlock)
            {
                //NAGOX_ASSERT(false, "Out of Memory");
                return nullptr;
            }
            const uint32_t newTag = unpackTag(oldCombined) + 1;
            Block* const nextBlock = unpackPointer(currentBlock->combined.load(std::memory_order_relaxed));
            newCombined = packPointerAndTag(nextBlock, newTag);
        } while (!head.compare_exchange_weak(oldCombined, newCombined,
            std::memory_order_acquire,
            std::memory_order_relaxed));

        return reinterpret_cast<T* const>(currentBlock + 1);
    }

    void deallocate(void* const object) noexcept
    {
        if (!object)
        {
            return;
        }
        Block* const blockPtr = reinterpret_cast<Block* const>(object) - 1;
        uint64_t oldHead = head.load(std::memory_order_relaxed);
        uint64_t newCombined;
        do {
            const uint32_t newTag = unpackTag(oldHead) + 1;
            newCombined = packPointerAndTag(blockPtr, newTag);
            blockPtr->combined.store(oldHead, std::memory_order_relaxed);
        } while (!head.compare_exchange_weak(oldHead, newCombined,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    void checkAndResetIfNeeded()noexcept
    {
        const uint32_t currentTag = unpackTag(head.load(std::memory_order_relaxed));
        if (currentTag >= maxTagValue)
        {
            initialize();
        }
    }

    const bool isNeedReset()const noexcept {
        return maxTagValue - 10000 <= unpackTag(head.load(std::memory_order_relaxed));
    }
};
struct Test
{
    double d[100];
    ~Test() { cout << "¼Ò¸ê" << endl; }
};

template <typename T>
class AtomicAllocater
{
private:
    static inline AtomicMemoryPool<T> memPool{ 5000 };
public:
    using value_type = T;

    AtomicAllocater(){}

    template <typename U>
    AtomicAllocater(const AtomicAllocater<U>&){}

    T* allocate(size_t count)
    {
        return static_cast<T*>(memPool.allocate());
    }

    void deallocate(T* ptr, size_t count)
    {
        memPool.deallocate(ptr);
    }
};


int main()
{
    vector < shared_ptr<Test>>vec;
    for (int i = 0; i < 1000; ++i)
        vec.emplace_back(allocate_shared<Test>(AtomicAllocater<Test>{}));

}