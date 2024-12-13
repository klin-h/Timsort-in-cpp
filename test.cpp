#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <random>
#include <chrono>
#include <string>
#include <functional>

template <typename RandomIt, typename Compare>
void quickSort(RandomIt first, RandomIt last, Compare comp) {
    if (first < last) {
        auto pivot = *(last - 1);
        RandomIt i = first;
        for (RandomIt j = first; j < last - 1; ++j) {
            if (comp(*j, pivot)) {
                std::swap(*i, *j);
                ++i;
            }
        }
        std::swap(*i, *(last - 1));
        quickSort(first, i, comp);
        quickSort(i + 1, last, comp);
    }
}


namespace timsort_detail {

    const int MIN_MERGE = 32;

    static int minRunLength(int n) {
        int r = 0;
        while (n >= MIN_MERGE) {
            r |= (n & 1);
            n >>= 1;
        }
        return n + r;
    }

    template <typename RandomIt, typename Compare>
    static void binaryInsertionSort(RandomIt left, RandomIt right, Compare comp) {
        for (auto it = left + 1; it < right; ++it) {
            auto key = std::move(*it);
     
            RandomIt pos = std::upper_bound(left, it, key, comp);
            if (pos != it) {
                std::move_backward(pos, it, it + 1);
                *pos = std::move(key);
            }
        }
    }


    template <typename RandomIt, typename Compare>
    static void mergeRuns(RandomIt start, RandomIt mid, RandomIt end, Compare comp, std::vector<typename std::iterator_traits<RandomIt>::value_type>& buffer) {
        using ValueType = typename std::iterator_traits<RandomIt>::value_type;
        int leftSize = static_cast<int>(mid - start);
        int rightSize = static_cast<int>(end - mid);

        if (buffer.size() < static_cast<size_t>(leftSize)) {
            buffer.resize(leftSize);
        }

        std::move(start, mid, buffer.begin());

        RandomIt left = buffer.begin();
        RandomIt leftEnd = buffer.begin() + leftSize;
        RandomIt right = mid;
        RandomIt rightEnd = end;
        RandomIt dest = start;

        while (left != leftEnd && right != rightEnd) {
            if (comp(*left, *right)) {
                *dest++ = std::move(*left++);
            }
            else {
                *dest++ = std::move(*right++);
            }
        }

        if (left != leftEnd) {
            std::move(left, leftEnd, dest);
        }
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
                }
                else {
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
                }
                else {
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
int main() {
    const int randomDataSize = 50000;
    const int specialDataSize = 1000;
    const int testIterations = 5;
    const int maxValue = 1000000;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, maxValue);

    std::vector<int> dataRandom(randomDataSize);
    std::vector<int> dataNearlySorted(specialDataSize);
    std::vector<int> dataManyRuns(specialDataSize);
    std::vector<int> dataReversed(specialDataSize);

    auto generateRandomData = [&](std::vector<int>& vec) {
        for (auto& val : vec) {
            val = dist(gen);
        }
        };

    auto generateNearlySortedData = [&](std::vector<int>& vec) {
        for (int i = 0; i < (int)vec.size(); ++i) {
            vec[i] = i;
        }
        std::uniform_int_distribution<int> swapDist(0, (int)vec.size() - 1);
        for (int i = 0; i < (int)vec.size() / 100; ++i) {
            int idx1 = swapDist(gen);
            int idx2 = swapDist(gen);
            std::swap(vec[idx1], vec[idx2]);
        }
        };

    auto generateManySmallRunsData = [&](std::vector<int>& vec) {
        int runSize = 100;
        int current = 0;
        while (current < (int)vec.size()) {
            int runEnd = std::min(current + runSize, (int)vec.size());
            for (int i = current; i < runEnd; ++i) {
                vec[i] = i;
            }
            std::shuffle(vec.begin() + current, vec.begin() + runEnd, gen);
            current = runEnd;
        }
        };

    auto generateReversedData = [&](std::vector<int>& vec) {
        for (int i = 0; i < (int)vec.size(); ++i) {
            vec[i] = (int)vec.size() - i;
        }
        };

    auto measureTime = [&](const std::function<void(std::vector<int>&)>& sortFunc, const std::string& name, const std::vector<int>& inputData) {
        long long totalTime = 0;
        for (int i = 0; i < testIterations; ++i) {
            std::vector<int> data = inputData;
            auto start = std::chrono::high_resolution_clock::now();
            sortFunc(data);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> elapsed = end - start;
            totalTime += (long long)elapsed.count();
        }
        double avgTime = (double)totalTime / testIterations;
        std::cout << name << ": Average time over " << testIterations << " runs: "
            << avgTime << " microseconds." << std::endl;
        };

    std::cout << "--- Random Data Test ---\n";
    generateRandomData(dataRandom);

    struct SortAlgorithm {
        std::string name;
        std::function<void(std::vector<int>&)> func;
    };

    std::vector<SortAlgorithm> sortingAlgorithms = {
        { "std::sort", [&](std::vector<int>& vec) { std::sort(vec.begin(), vec.end()); } },
        { "std::stable_sort", [&](std::vector<int>& vec) { std::stable_sort(vec.begin(), vec.end()); } },
        { "Timsort", [&](std::vector<int>& vec) { timsort(vec.begin(), vec.end(), std::less<int>()); } },
        { "QuickSort", [&](std::vector<int>& vec) { quickSort(vec.begin(), vec.end(), std::less<int>()); } },
    };

    for (const auto& algo : sortingAlgorithms) {
        measureTime(algo.func, algo.name, dataRandom);
    }

    std::cout << "\n--- Special Test Cases ---\n";

    generateNearlySortedData(dataNearlySorted);
    std::cout << "\nSpecial Test Case: Nearly Sorted Data\n";
    for (const auto& algo : sortingAlgorithms) {
        measureTime(algo.func, algo.name, dataNearlySorted);
    }

    generateManySmallRunsData(dataManyRuns);
    std::cout << "\nSpecial Test Case: Many Small Runs Data\n";
    for (const auto& algo : sortingAlgorithms) {
        measureTime(algo.func, algo.name, dataManyRuns);
    }

    generateReversedData(dataReversed);
    std::cout << "\nSpecial Test Case: Reversed Data\n";
    for (const auto& algo : sortingAlgorithms) {
        measureTime(algo.func, algo.name, dataReversed);
    }

    return 0;
}
