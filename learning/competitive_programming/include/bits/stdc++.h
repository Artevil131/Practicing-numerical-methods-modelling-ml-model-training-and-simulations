// Shim so `#include <bits/stdc++.h>` works with Apple clang / libc++.
// Compile with:  c++ -std=c++17 -O2 -I competitive_programming/include prog.cpp
// (GCC's libstdc++ ships this header; libc++ doesn't. On judges it exists — this file is local only.)
// NOTE: libstdc++-only extensions are still absent: bitset::_Find_first/_Find_next, <ext/pb_ds/...>.
//       For those use `brew install gcc` and compile with g++-14.
#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cctype>
#include <chrono>
#include <climits>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
