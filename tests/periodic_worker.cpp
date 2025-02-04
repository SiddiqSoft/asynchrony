/*
    asynchrony-lib
    Add asynchrony to your apps

    BSD 3-Clause License

    Copyright (c) 2021, Siddiq Software LLC
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gtest/gtest.h"

#include <iostream>
#include <format>
#include <string>
#include <thread>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/periodic_worker.hpp"


TEST(periodic_worker, test1)
{
    uint64_t                    passTest {0};

    siddiqsoft::periodic_worker worker {[&]() { passTest++; }, std::chrono::milliseconds(100)};

    while (passTest <= 44)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // We expect at least 50 iterations at a rate of one per 100ms with a test time of 5s.
    // This test is going to be tough since each VM configuration varies and the CPU frequency has a bearing on how "fast" or "lazy"
    // the wait on the semaphore is accurate.
    EXPECT_LE(44, passTest);

    std::cerr << worker.toJson().dump() << std::endl;
}

TEST(periodic_worker, nosleep_test2)
{
    uint64_t                    passTest {0};

    siddiqsoft::periodic_worker worker {[&]() {
                                            // this sleep will force the worker to terminate mid-call
                                            std::cerr << "  Started......`" << __func__ << "`......" << std::endl;
                                            std::this_thread::sleep_for(std::chrono::seconds(2));
                                            passTest++;
                                            std::cerr << "  Completed....`" << __func__ << "`......" << std::endl;
                                        },
                                        // run the above code every 50ms
                                        std::chrono::milliseconds(50)};

    // We expect at least one iteration completed.
    EXPECT_GE(1, passTest) << std::format("At least one iteration; completed: {}", passTest);

    std::clog << worker.toJson().dump() << std::endl;
}
