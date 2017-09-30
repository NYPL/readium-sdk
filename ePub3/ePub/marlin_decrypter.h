//
//  marlin_decrypter.h
//  ePub3
//
//  Created by yano on 2014/04/28.
//  Copyright (c) 2014年 The Readium Foundation and contributors. All rights reserved.
//

#ifndef __ePub3__marlin_decrypter__
#define __ePub3__marlin_decrypter__

#include <ePub3/filter.h>
#include <ePub3/encryption.h>
#include <ePub3/utilities/byte_stream.h>
#include <ePub3/zip_archive.h>

#include REGEX_INCLUDE
#include <cstring>

#define MARLIN_DECRYPTOR_FILTER_ID "EBCFD2F5-0460-41E0-91AA-5C1EBC1ACB06"
#define MARLIN_RIGHTS_ID "<Marlin xmlns=\"http://marlin-drm.com/epub\">"

EPUB3_BEGIN_NAMESPACE

class MarlinDecrypter : public ContentFilter, public PointerType<MarlinDecrypter>
{
private:
    ConstPackagePtr _package;

    static bool SniffMarlinContent(ConstManifestItemPtr item);
    static ContentFilterPtr MarlinDecrypterFactory(ConstPackagePtr package);

    class MarlinDecrypterContext : public RangeFilterContext
    {
    private:
        const std::string   _absolutePath;
        const std::string   _relativePath;
    public:
        MarlinDecrypterContext(const std::string absolutePath, const std::string relativePath) : RangeFilterContext(), _absolutePath(absolutePath), _relativePath(relativePath) {}
        virtual ~MarlinDecrypterContext() {}
        const std::string& getRelativePath() { return _relativePath; }
        const std::string& getAbsolutePath() { return _absolutePath; }
    };

public:
    MarlinDecrypter(ConstPackagePtr package) : ContentFilter(SniffMarlinContent)
    {
        _package = package;
    }
    ///
    /// Copy constructor.
    MarlinDecrypter(const MarlinDecrypter& o) : ContentFilter(o)
    {
        _package = o._package;
    }
    ///
    /// Move constructor.
    MarlinDecrypter(MarlinDecrypter&& o) : ContentFilter(std::move(o))
    {
        _package = o._package;
    }
 
    virtual void * FilterData(FilterContext* context, void *data, size_t len, size_t *outputLen) OVERRIDE;
    
    static void Register();
    OperatingMode GetOperatingMode() const OVERRIDE { return OperatingMode::SupportsByteRanges; }
    virtual ByteStream::size_type BytesAvailable(FilterContext *context, SeekableByteStream *byteStream) const override;

protected:
    virtual FilterContext *InnerMakeFilterContext(ConstManifestItemPtr item) const OVERRIDE;
};

EPUB3_END_NAMESPACE

#endif /* defined(__ePub3__marlin_decrypter__) */
