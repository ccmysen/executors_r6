# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

######## General Configuration

#### Source Locations

VPATH = ../src ../test ../third_party/googletest/src

#### Compilation Flags

DFlags = -MMD -MP
EFlags = -Wall -Werror
IFlags = -I. -I../include
BFlags = $(DFlags) $(EFlags) $(IFlags) $(BFLAGS)
CFlags = $(BFlags) $(CFLAGS)
CxxFlags = $(BFlags) $(CXXFLAGS)
LFlags = $(CXXFLAGS)

#### Compilation Rules

%.o : %.c
	$(CC) $(CFlags) $< -c -o $*.o 

%.o : %.cc
	$(CXX) $(CxxFlags) $< -c -o $*.o 

%.a :
	/bin/rm -f $@
	/usr/bin/ar -rc $@ $^

%.exe :
	$(CXX) $^ $(LFlags) -o $*.exe

%.pass : %.exe
	sh ../util/exetest.sh $*

-include *.d

#### GTest Builds

GTEST_INC := -I../third_party/googletest/include
GTEST_SRC := -I../third_party/googletest
GTEST_OBJ := gtest-all.o gtest_main.o
$(GTEST_OBJ) : CxxFlags += $(GTEST_INC) $(GTEST_SRC)

######## Library Build

######## Tests

EXECUTOR_TESTS := executor_test.o executor_impl_test.o
$(EXECUTOR_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
executor_test.exe : $(EXECUTOR_TESTS) $(GTEST_OBJ)
executor_test.run : executor_test.exe

PARALLELISM_TESTS := spawn_test.o
$(PARALLELISM_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
parallelism_test.exe : $(PARALLELISM_TESTS) $(GTEST_OBJ)
parallelism_test.run : paralellism_test.exe

######## Testing

test : parallelism_test.pass executor_test.pass


