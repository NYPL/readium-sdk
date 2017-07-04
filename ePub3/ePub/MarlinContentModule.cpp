// Licensed to the Readium Foundation under one or more contributor license agreements.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation and/or
//    other materials provided with the distribution.
// 3. Neither the name of the organization nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "MarlinContentModule.h"
#include "marlin_decrypter.h"
#include <ePub3/container.h>
#include <ePub3/content_module_exception.h>
#include <ePub3/content_module_manager.h>
#include <ePub3/utilities/byte_stream.h>
#include <ePub3/utilities/utfstring.h>

#if DEBUG
#define LOG(msg) std::cout << "MARLIN: " << msg << std::endl
#else
#define LOG(msg)
#endif

    ePub3::string MarlinContentModule::GetModuleName() {
        return ePub3::string("marlin");
    }

    void MarlinContentModule::RegisterContentFilters() {
        MarlinDecrypter::Register();
    }
#if FUTURE_ENABLED
    async_result<ContainerPtr>
#else
    ContainerPtr
#endif //FUTURE_ENABLED
    MarlinContentModule::ProcessFile(const ePub3::string &path
#if FUTURE_ENABLED
            , ePub3::launch policy
#endif //FUTURE_ENABLED
                                  ) {
        ContainerPtr container2 = Container::OpenContainerForContentModule(path); //, true);

        if(container2 == nullptr) {

#if FUTURE_ENABLED
            return make_ready_future<ContainerPtr>(ContainerPtr(nullptr));
#else
            return nullptr;
#endif //FUTURE_ENABLED

        }

          auto stream = container2->ReadStreamAtPath("META-INF/rights.xml");
          if (stream == nullptr)
          {
#if FUTURE_ENABLED
              return make_ready_future<ContainerPtr>(ContainerPtr(nullptr));
#else
              return nullptr;
#endif //FUTURE_ENABLED
          }
          
          ssize_t file_size = stream->BytesAvailable();
          if (file_size == 0) {
#if FUTURE_ENABLED
              return make_ready_future<ContainerPtr>(ContainerPtr(nullptr));
#else
              return nullptr;
#endif //FUTURE_ENABLED
          }
          unsigned char *file_content = (unsigned char *)malloc(file_size + 1);
          file_content[file_size] = 0;
          
          ssize_t bytes_read = stream->ReadBytes(file_content, file_size);
          if (bytes_read != file_size)
          {
              free(file_content);
#if FUTURE_ENABLED
              return make_ready_future<ContainerPtr>(ContainerPtr(nullptr));
#else
              return nullptr;
#endif //FUTURE_ENABLED
          }
          
          if ( std::string(reinterpret_cast<const char*>(file_content)).find(MARLIN_RIGHTS_ID) == std::string::npos )
          {
              free(file_content);
#if FUTURE_ENABLED
              return make_ready_future<ContainerPtr>(ContainerPtr(nullptr));
#else
              return nullptr;
#endif //FUTURE_ENABLED
          }

          free(file_content);

#if FUTURE_ENABLED
       return make_ready_future<ContainerPtr>(ContainerPtr(container2));
#else
        return container2;
#endif //FUTURE_ENABLED
    }

#if FUTURE_ENABLED
    async_result<bool> MarlinContentModule::ApproveUserAction(const UserAction &action) {
        async_result<bool> result;
        return result;
    }
#endif //FUTURE_ENABLED

    void MarlinContentModule::Register() {
        MarlinContentModule *contentModule = new MarlinContentModule();
        ContentModuleManager::Instance()->RegisterContentModule(contentModule, ePub3::string("MarlinContentModule")); //_NOEXCEPT
    }
