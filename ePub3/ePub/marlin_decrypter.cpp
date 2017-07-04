
//
//  marlin_decrypter.cpp
//  ePub3
//
//  Created by yano on 2014/04/28.
//  Copyright (c) 2014年 The Readium Foundation and contributors. All rights reserved.
//

#include <ePub3/base.h>
#include "marlin_decrypter.h"
#include "container.h"
#include "package.h"
#include "filter_manager.h"

#include "../ThirdParty/marlin-sdk/marlin_sdk.h"

EPUB3_BEGIN_NAMESPACE

using namespace MarlinSDK;

const char *MarlinEncryptionAlgorithmID = "http://www.w3.org/2001/04/xmlenc#aes128-cbc";
std::string MarlinRightsID = "<Marlin xmlns=\"http://marlin-drm.com/epub\">";

class MarlinStreamImpl : public MarlinStream
{
public:
    MarlinStreamImpl(SeekableByteStream *stream);
    ~MarlinStreamImpl();
    bool        isOpened() {return this->resource ? true:false;}
    uint32_t    GetLength();
    bool        Seek(uint32_t position);
    uint32_t    Read(void *buffer, uint32_t length);
    bool        IsValid();

private:
    SeekableByteStream  *resource;
    SeekableByteStream::size_type            oldPos;
    bool        error;
};

bool MarlinStreamImpl::IsValid()
{
    return !error;
}

MarlinStreamImpl::MarlinStreamImpl(SeekableByteStream *stream)
{
    if (stream) {
        this->resource = stream;
        this->oldPos = stream->Position();
        this->Seek(0);
        this->error = !stream->IsOpen();
    }
    else    {
        this->resource = nullptr;
        this->error = true;
    }
}

MarlinStreamImpl::~MarlinStreamImpl()
{
    if (this->resource) {
        this->resource->Seek(this->oldPos, std::ios::beg);
    }
}

uint32_t MarlinStreamImpl::GetLength()
{
    ByteStream::size_type    length = 0;
    if (this->resource) {
        ByteStream::size_type actualPosition = this->resource->Position();
        this->resource->Seek(0, std::ios::end);
        length = this->resource->Position();
        this->resource->Seek(actualPosition, std::ios::beg);
    }
    return (uint32_t)length;
}

bool MarlinStreamImpl::Seek(uint32_t position)
{
    if (this->resource) {
        ByteStream::size_type pos = this->resource->Seek(position, std::ios::beg);
        if (pos == position)    {
            return true;
        }
    }
    this->error = true;
    return false;
}

uint32_t MarlinStreamImpl::Read(void *buffer, uint32_t length)
{
    if (this->resource) {
        return (uint32_t)this->resource->ReadBytes(buffer, length);
    }
    this->error = true;
    return 0;
}

bool MarlinDecrypter::SniffMarlinContent(ConstManifestItemPtr item)
{
    if (item->GetEncryptionInfo() && item->GetEncryptionInfo()->Algorithm() == MarlinEncryptionAlgorithmID)  {
        return true;
    }
    return false;
}

ContentFilterPtr MarlinDecrypter::MarlinDecrypterFactory(ConstPackagePtr package)
{
    ConstContainerPtr container = package->GetContainer();

    auto stream = container->ReadStreamAtPath("META-INF/rights.xml");
    if (stream == nullptr)
    {
      return nullptr;
    }

    ssize_t file_size = stream->BytesAvailable();
    if (file_size == 0) {
        return nullptr;
    }
    unsigned char *file_content = (unsigned char *)malloc(file_size + 1);
    file_content[file_size] = 0;

    ssize_t bytes_read = stream->ReadBytes(file_content, file_size);
    if (bytes_read != file_size)
    {
        free(file_content);
        return nullptr;
    }

    if ( std::string(reinterpret_cast<const char*>(file_content)).find(MarlinRightsID) != std::string::npos )
    {
        free(file_content);
        return New(package);
    }

    free(file_content);
    // opted out, nothing for us to do here
    return nullptr;
}

FilterContext *MarlinDecrypter::InnerMakeFilterContext(ConstManifestItemPtr item) const
{
    const std::string base_path = item->GetPackage()->GetContainer()->Path().c_str();
    const std::string path = item->AbsolutePath().c_str(); // needed for the next version of Marlin filter
    return new MarlinDecrypterContext(base_path, path);
}

void *MarlinDecrypter::FilterData(FilterContext *context, void *data, size_t len, size_t *outputLen)
{
    *outputLen = 0;
    MarlinDecrypterContext *mdContext = (MarlinDecrypterContext*)context;
    MarlinSDK::MarlinContextPtr marlinContext = MarlinSDK::MarlinContext::CreateMarlinContext();

    if (mdContext == nullptr)   {
        return nullptr;
    }

    SeekableByteStream *byteStream = mdContext->GetSeekableByteStream();
    if (byteStream == nullptr)  { // Classic filtering in the chain of more filters
        auto sourceMemoryStream = marlinContext->CreateStreamFromBuffer(data, (uint32_t)len);
        auto decryptStream = marlinContext->CreateDecryptor(sourceMemoryStream);

        if (marlinContext->GetLastError().IsError())    {
            std::ostringstream ostr;
            ostr << "Marlin internal error (" << marlinContext->GetLastError().CodeStr() << ")";
            HandleContentFilterError(std::string(MARLIN_DECRYPTOR_FILTER_ID), ContentFilterError::GenericError, ostr.str());
            return nullptr;
        }

        uint32_t readSize = decryptStream->GetLength();
        uint8_t *buffer = mdContext->GetAllocateTemporaryByteBuffer(readSize);

        ByteStream::size_type readBytes = decryptStream->Read(buffer, readSize);

        if (readBytes != readSize)   {
            *outputLen = 0;
            return nullptr;
        }

        memcpy(data, buffer, readBytes);
        *outputLen = readBytes;
        return data;
    }

    if (!byteStream->IsOpen())  {
        return nullptr;
    }

    ByteStream::size_type bytesToRead = 0;
    shared_ptr<MarlinStreamImpl>    fileContainer = shared_ptr<MarlinStreamImpl>(new MarlinStreamImpl(byteStream));
    if (!fileContainer->isOpened())  {
        HandleContentFilterError(std::string(MARLIN_DECRYPTOR_FILTER_ID), ContentFilterError::InputStreamCannotBeOpened, std::string("MarlinDecrypt failed, cannot open input stream"));
        return nullptr;
    }

//    uint32 length = mdContext->GetByteRange().Length();
//    if (fileContainer->GetLength() < mdContext->GetByteRange().Location() + mdContext->GetByteRange().Length())   { // check if there is enough data for the request
//        length = fileContainer->GetLength() - mdContext->GetByteRange().Location();
//    }

    auto decryptStream = marlinContext->CreateDecryptor(fileContainer);

    if (marlinContext->GetLastError().IsError())    {
        std::ostringstream ostr;
        ostr << "Marlin internal error (" << marlinContext->GetLastError().CodeStr() << ")";
        HandleContentFilterError(std::string(MARLIN_DECRYPTOR_FILTER_ID), ContentFilterError::GenericError, ostr.str());
        return nullptr;

    }

    if (!mdContext->GetByteRange().IsFullRange())   { // range requests only
        bool seekRet = decryptStream->Seek(mdContext->GetByteRange().Location());
        if (!seekRet)   {
            HandleContentFilterError(std::string(MARLIN_DECRYPTOR_FILTER_ID), ContentFilterError::OutputStreamCannotBeSought, std::string("MarlinDecrypt failed, cannot seek inside output stream"));
            return nullptr;
        }

//        bytesToRead = std::min(mdContext->GetByteRange().Length(), decryptStream->GetLength() - mdContext->GetByteRange().Location());
        bytesToRead = std::min(mdContext->GetByteRange().Length(), decryptStream->GetLength() - mdContext->GetByteRange().Location());
    }
    else    { // Full-range request
        bool seekRet = decryptStream->Seek(0);
        if (!seekRet)   {
            HandleContentFilterError(std::string(MARLIN_DECRYPTOR_FILTER_ID), ContentFilterError::OutputStreamCannotBeSought, std::string("MarlinDecrypt failed, cannot seek inside output stream"));
            return nullptr;
        }

        bytesToRead = decryptStream->GetLength();
    }

    if (bytesToRead == 0)   {
        *outputLen = 0;
        return nullptr;
    }

    uint8_t *buffer = mdContext->GetAllocateTemporaryByteBuffer(bytesToRead);

    ByteStream::size_type readBytes = decryptStream->Read(buffer, (uint32_t)bytesToRead);
    *outputLen = readBytes;
    return buffer;
}

void MarlinDecrypter::Register()
{
    FilterManager::Instance()->RegisterFilter("MarlinDecrypter", EPUBDecryption + 1, MarlinDecrypterFactory);
}

ByteStream::size_type MarlinDecrypter::BytesAvailable(FilterContext *context, SeekableByteStream *byteStream) const
{
    if (!byteStream->IsOpen())  {
        return 0;
    }

    MarlinSDK::MarlinContextPtr marlinContext = MarlinSDK::MarlinContext::CreateMarlinContext();

    shared_ptr<MarlinStreamImpl>    fileContainer = shared_ptr<MarlinStreamImpl>(new MarlinStreamImpl(byteStream));
    if (!fileContainer->isOpened())  {
        return 0;
    }

    auto decryptStream = marlinContext->CreateDecryptor(fileContainer);

    if (marlinContext->GetLastError().IsError())    {
        return 0;
    }

    return decryptStream->GetLength();
}


EPUB3_END_NAMESPACE
