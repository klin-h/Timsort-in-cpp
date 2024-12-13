#include <algorithm>
#include <iterator>
#include <vector>
#include <functional>
#include <cassert>

namespace timsort_detail {

    const int MIN_MERGE = 32;

    // 计算最小运行长度
    static int minRunLength(int n) {
        int r = 0;
        while (n >= MIN_MERGE) {
            r |= (n & 1);
            n >>= 1;
        }
        return n + r;
    }

    // 二分插入排序，优化移动操作
    template <typename RandomIt, typename Compare>
    static void binaryInsertionSort(RandomIt left, RandomIt right, Compare comp) {
        for (auto it = left + 1; it < right; ++it) {
            auto key = std::move(*it);
            // 查找插入位置
            RandomIt pos = std::upper_bound(left, it, key, comp);
            // 如果插入位置不是当前元素位置，执行移动
            if (pos != it) {
                std::move_backward(pos, it, it + 1);
                *pos = std::move(key);
            }
        }
    }

    // 合并两个已排序的运行，加入跳跃模式
    template <typename RandomIt, typename Compare>
    static void mergeRuns(RandomIt start, RandomIt mid, RandomIt end, Compare comp, std::vector<typename std::iterator_traits<RandomIt>::value_type>& buffer) {
        using ValueType = typename std::iterator_traits<RandomIt>::value_type;
        int leftSize = static_cast<int>(mid - start);
        int rightSize = static_cast<int>(end - mid);

        // 确保缓冲区足够大
        if (buffer.size() < static_cast<size_t>(leftSize)) {
            buffer.resize(leftSize);
        }

        // 将左半部分复制到缓冲区
        std::move(start, mid, buffer.begin());

        RandomIt left = buffer.begin();
        RandomIt leftEnd = buffer.begin() + leftSize;
        RandomIt right = mid;
        RandomIt rightEnd = end;
        RandomIt dest = start;

        // 实现跳跃模式
        while (left != leftEnd && right != rightEnd) {
            if (comp(*left, *right)) {
                *dest++ = std::move(*left++);
            } else {
                *dest++ = std::move(*right++);
            }
        }

        // 复制剩余的左半部分
        if (left != leftEnd) {
            std::move(left, leftEnd, dest);
        }
        // 右半部分的元素已经在原位置，无需复制
    }

    struct Run {
        int start;
        int length;
    };

    template <typename RandomIt, typename Compare>
    void timsortImpl(RandomIt first, RandomIt last, Compare comp) {
        int n = static_cast<int>(std::distance(first, last));
        if (n <= 1) return;

        int minRun = minRunLength(n);
        std::vector<Run> runStack;
        runStack.reserve(64); // 预分配足够的空间

        std::vector<typename std::iterator_traits<RandomIt>::value_type> buffer;
        buffer.reserve(minRun); // 预分配缓冲区

        int start = 0;
        while (start < n) {
            int runLen = 1;

            // 检测运行方向
            if (start + 1 < n) {
                if (comp(*(first + start + 1), *(first + start))) {
                    // 降序运行，反转为升序
                    while (start + runLen < n && comp(*(first + start + runLen), *(first + start + runLen - 1))) {
                        runLen++;
                    }
                    std::reverse(first + start, first + start + runLen);
                } else {
                    // 升序运行
                    while (start + runLen < n && !comp(*(first + start + runLen), *(first + start + runLen - 1))) {
                        runLen++;
                    }
                }
            }

            // 如果运行长度小于最小运行长度，进行扩展
            if (runLen < minRun) {
                int force = std::min(minRun, n - start);
                binaryInsertionSort(first + start, first + start + force, comp);
                runLen = force;
            }

            // 将当前运行压入堆栈
            runStack.push_back(Run{ start, runLen });

            // 合并运行，维护 Timsort 的堆栈不变量
            while (runStack.size() > 1) {
                int size = static_cast<int>(runStack.size());
                bool shouldMerge = false;

                if (size >= 3) {
                    int A = runStack[size - 3].length;
                    int B = runStack[size - 2].length;
                    int C = runStack[size - 1].length;
                    if (A <= B + C) {
                        shouldMerge = true;
                    }
                }
                if (!shouldMerge && size >= 2) {
                    int B = runStack[size - 2].length;
                    int C = runStack[size - 1].length;
                    if (B <= C) {
                        shouldMerge = true;
                    }
                }

                if (shouldMerge) {
                    // 弹出最后两个运行进行合并
                    Run run2 = runStack.back(); runStack.pop_back();
                    Run run1 = runStack.back(); runStack.pop_back();

                    mergeRuns(first + run1.start, first + run1.start + run1.length,
                              first + run1.start + run1.length + run2.length, comp, buffer);

                    // 将合并后的运行压入堆栈
                    runStack.push_back(Run{ run1.start, run1.length + run2.length });
                } else {
                    break;
                }
            }

            start += runLen;
        }

        // 最终合并所有运行
        while (runStack.size() > 1) {
            // 弹出最后两个运行进行合并
            Run run2 = runStack.back(); runStack.pop_back();
            Run run1 = runStack.back(); runStack.pop_back();

            mergeRuns(first + run1.start, first + run1.start + run1.length,
                      first + run1.start + run1.length + run2.length, comp, buffer);

            // 将合并后的运行压入堆栈
            runStack.push_back(Run{ run1.start, run1.length + run2.length });
        }
    }

} // namespace timsort_detail

// 对外接口，简化使用
template <typename RandomIt, typename Compare = std::less<typename std::iterator_traits<RandomIt>::value_type>>
void timsort(RandomIt first, RandomIt last, Compare comp = Compare()) {
    timsort_detail::timsortImpl(first, last, comp);
}
