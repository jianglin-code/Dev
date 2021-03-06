/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "shared.rsh"

struct Simple {
    int I;
    long L;
};

struct Simple simple;

void checkSimple(int argI, long argL) {
    bool failed = false;

    rsDebug("argI       ", argI);
    rsDebug("simple.I   ", simple.I);
    rsDebug("argL.lo    ", (unsigned)argL & ~0U);
    rsDebug("simple.L.lo", (unsigned)simple.L & ~0U);
    rsDebug("argL.hi    ", (unsigned)((ulong)argL >> 32));
    rsDebug("simple.L.hi", (unsigned)((ulong)simple.L >> 32));

    _RS_ASSERT(simple.I == argI);
    _RS_ASSERT(simple.L == argL);

    if (failed) {
        rsDebug("struct_field_simple FAILED", 0);
        rsSendToClientBlocking(RS_MSG_TEST_FAILED);
    }
    else {
        rsDebug("struct_field_simple PASSED", 0);
        rsSendToClientBlocking(RS_MSG_TEST_PASSED);
    }
}
