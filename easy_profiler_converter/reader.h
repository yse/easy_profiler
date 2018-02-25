/**
Lightweight profiler library for c++
Copyright(C) 2016-2018  Sergey Yagovtsev, Victor Zarubkin

Licensed under either of
	* MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
    * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
at your option.

The MIT License
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
	of the Software, and to permit persons to whom the Software is furnished
	to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
	INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
	PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
	LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	USE OR OTHER DEALINGS IN THE SOFTWARE.


The Apache License, Version 2.0 (the "License");
	You may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.

**/

#ifndef EASY_PROFILER_READER_H
#define EASY_PROFILER_READER_H

///std
#include <fstream>
#include <vector>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

///this
#include <easy/easy_protocol.h>
#include <easy/reader.h>
#include <easy/utility.h>

namespace profiler {

namespace reader {

class BlocksTreeNode;
using thread_blocks_tree_t = ::std::unordered_map<::profiler::thread_id_t, BlocksTreeNode, ::estd::hash<::profiler::thread_id_t> >;
using thread_names_t = ::std::unordered_map<::profiler::thread_id_t, ::std::string>;
using context_switches_t = ::std::vector<::std::shared_ptr<ContextSwitchEvent> >;

class BlocksTreeNode
{
public:
    ::std::vector<::std::shared_ptr<BlocksTreeNode>> children;
    ::std::shared_ptr<BlockInfo> current_block;
    BlocksTreeNode* parent;

    BlocksTreeNode(BlocksTreeNode&& other) : children(::std::move(other.children))
        , current_block(::std::move(other.current_block))
        ,  parent(other.parent)
    {
    }

    BlocksTreeNode() : parent(nullptr)
    {
    }
}; // end of class BlocksTreeNode.

class FileReader EASY_FINAL
{
public:

    /*-----------------------------------------------------------------*/
    ///initial read file with RAW data
    ::profiler::block_index_t readFile(const ::std::string& filename);
    /*-----------------------------------------------------------------*/
    ///get blocks tree
    const thread_blocks_tree_t& getBlocksTreeData();
    /*-----------------------------------------------------------------*/
    /*! get thread name by Id
    \param threadId thread Id
    \return Name of thread
    */
    const std::string &getThreadName(uint64_t threadId);
    /*-----------------------------------------------------------------*/
    /*! get file version
    \return data file version
    */
    uint32_t getVersion();
    /*-----------------------------------------------------------------*/
    ///get sontext switches
    const context_switches_t& getContextSwitches();
    /*-----------------------------------------------------------------*/

private:

    ///serialized raw data
    ::profiler::SerializedData                             serialized_blocks, serialized_descriptors;
    ///error log stream
    ::std::stringstream                                    errorMessage;
    ///thread's blocks
    thread_blocks_tree_t                                   m_BlocksTree;
    ///[thread_id, thread_name]
    thread_names_t                                         m_threadNames;
    ///context switches info
    context_switches_t                                     m_ContextSwitches;
    std::vector<std::shared_ptr<BlockDescriptor>>          m_BlockDescriptors;
    ///data file version
    uint32_t                                               m_version;

}; // end of class FileReader.

} // end of namespace reader.

} // end of namespace profiler.

#endif // EASY_PROFILER_READER_H
