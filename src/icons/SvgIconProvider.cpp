#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "SvgIconProvider.h"
#include "IconBitmapUtils.h"
#include "DpiProvider.h"
#include "PathHelpers.h"
#include "ShellLog.h"
#include <algorithm>
#include <vector>

#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

namespace
{
    bool ReadUtf8FileToBytes(const std::wstring& path, std::vector<char>& bytes)
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr)
        {
            return false;
        }

        if (fseek(file, 0, SEEK_END) != 0)
        {
            fclose(file);
            return false;
        }

        const long fileSize = ftell(file);
        if (fileSize <= 0)
        {
            fclose(file);
            return false;
        }

        if (fseek(file, 0, SEEK_SET) != 0)
        {
            fclose(file);
            return false;
        }

        bytes.resize(static_cast<size_t>(fileSize));
        if (fread(bytes.data(), 1, bytes.size(), file) != bytes.size())
        {
            fclose(file);
            return false;
        }

        fclose(file);
        return true;
    }
}

bool SvgIconProvider::CanProvide(const std::wstring& iconSpec) const
{
    return HasExtensionIgnoreCase(iconSpec, L".svg");
}

HBITMAP SvgIconProvider::LoadIcon(
    const std::wstring& iconSpec,
    UINT dpi,
    HINSTANCE module)
{
    const std::wstring path = ResolveModuleRelativePath(module, iconSpec);
    std::vector<char> svgBytes;
    if (!ReadUtf8FileToBytes(path, svgBytes))
    {
        ShellLog(L"SvgIconProvider: failed to read %s", path.c_str());
        return nullptr;
    }

    svgBytes.push_back('\0');
    NSVGimage* image = nsvgParse(svgBytes.data(), "px", 96.0f);
    if (image == nullptr)
    {
        ShellLog(L"SvgIconProvider: failed to parse %s", path.c_str());
        return nullptr;
    }

    const int targetSize = DpiProvider::ScaleForDpi(DpiProvider::kBaseMenuIconSize, dpi);
    const float scaleX = image->width > 0.0f ? static_cast<float>(targetSize) / image->width : 1.0f;
    const float scaleY = image->height > 0.0f ? static_cast<float>(targetSize) / image->height : 1.0f;
    const float scale = (std::min)(scaleX, scaleY);
    const int width = (std::max)(1, static_cast<int>(image->width * scale));
    const int height = (std::max)(1, static_cast<int>(image->height * scale));

    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    if (rasterizer == nullptr)
    {
        nsvgDelete(image);
        return nullptr;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    nsvgRasterize(
        rasterizer,
        image,
        0.0f,
        0.0f,
        scale,
        pixels.data(),
        width,
        height,
        width * 4);

    HBITMAP bitmap = CreateBitmapFromRgba(width, height, pixels.data());
    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(image);

    if (bitmap == nullptr)
    {
        ShellLog(L"SvgIconProvider: rasterization failed for %s", path.c_str());
    }

    return bitmap;
}