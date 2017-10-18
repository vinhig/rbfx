//
// Copyright (c) 2008-2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../../Precompiled.h"

#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/VertexBuffer.h"
#include "../../IO/Log.h"

#include "../../DebugNew.h"

namespace Urho3D
{

    void VertexBuffer::OnDeviceLost()
    {

    }

    void VertexBuffer::OnDeviceReset()
    {

    }

    void VertexBuffer::Release()
    {
        if (object_.ptr_ != nullptr)
        {
            delete[] object_.ptr_;
            object_.ptr_ = nullptr;
        }
    }

    bool VertexBuffer::SetData(const void* data)
    {
        //memcpy(object_.ptr_, data, count);
        return false;
    }

    bool VertexBuffer::SetDataRange(const void* data, unsigned start, unsigned count, bool discard)
    {
        memcpy(&((char*)object_.ptr_)[start], data, count);
        return true;
    }

    void* VertexBuffer::Lock(unsigned start, unsigned count, bool discard)
    {
        return &((char*)object_.ptr_)[start];
    }

    void VertexBuffer::Unlock()
    {

    }

    bool VertexBuffer::Create()
    {
        Release();

        if (!vertexCount_ || elements_.Empty())
            return true;

        if (graphics_)
        {
            size_t memSize = vertexCount_ * vertexSize_;
            object_.ptr_ = new char[memSize];
        }
        return true;
    }

    bool VertexBuffer::UpdateToGPU()
    {
        return true;
    }

    void* VertexBuffer::MapBuffer(unsigned start, unsigned count, bool discard)
    {
        return &((char*)object_.ptr_)[start];
    }

    void VertexBuffer::UnmapBuffer()
    {
    }

}
