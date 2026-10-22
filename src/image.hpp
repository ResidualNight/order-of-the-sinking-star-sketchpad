#pragma once
#include <windows.h>
#include <wincodec.h>
#include "grid.hpp"
namespace sketch {
inline Image readImage(const wchar_t*path){Image image;IWICImagingFactory*factory=nullptr;IWICBitmapDecoder*decoder=nullptr;IWICBitmapFrameDecode*frame=nullptr;IWICFormatConverter*converter=nullptr;
 CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return image;
 if(SUCCEEDED(factory->CreateDecoderFromFilename(path,nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))&&SUCCEEDED(decoder->GetFrame(0,&frame))&&SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))){UINT w,h;converter->GetSize(&w,&h);image.width=w;image.height=h;image.rgba.resize(size_t(w)*h*4);if(FAILED(converter->CopyPixels(nullptr,w*4,(UINT)image.rgba.size(),image.rgba.data())))image={};}
 if(converter)converter->Release();if(frame)frame->Release();if(decoder)decoder->Release();factory->Release();return image;
}
}