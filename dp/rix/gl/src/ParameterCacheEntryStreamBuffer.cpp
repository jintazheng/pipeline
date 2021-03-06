// Copyright (c) 2013-2016, NVIDIA CORPORATION. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include <dp/rix/gl/inc/ParameterCacheEntryStreamBuffer.h>

namespace dp
{
  namespace rix
  {
    namespace gl
    {
      ParameterCacheEntryStreamBuffer::ParameterCacheEntryStreamBuffer( size_t cacheOffset, size_t containerOffset, size_t size )
        : m_cacheOffset( cacheOffset)
        , m_containerOffset( containerOffset)
        , m_size( size )
      {
      }

      template <int n, int m, typename SourceType, typename DestType>
      class CacheEntryMatrix : public ParameterCacheEntryStreamBuffer
      {
      public:
        static std::shared_ptr<CacheEntryMatrix> create(dp::gl::Program::Uniform const& uniformInfo, size_t containerOffset, size_t size);
        virtual void update( void * cache, void const * container ) const;

        size_t getSize() const { return m_size; }

      protected:
        CacheEntryMatrix( dp::gl::Program::Uniform const& uniformInfo, size_t containerOffset, size_t size );

      protected:
        size_t m_arraySize;
        size_t m_arrayStride;
        size_t m_matrixStride;
      };

      template <int n, int m, typename SourceType, typename DestType>
      std::shared_ptr<CacheEntryMatrix<n, m, SourceType, DestType>> CacheEntryMatrix<n, m, SourceType, DestType>::create(dp::gl::Program::Uniform const& uniformInfo, size_t containerOffset, size_t size)
      {
        return( std::shared_ptr<CacheEntryMatrix<n,m,SourceType,DestType>>( new CacheEntryMatrix<n,m,SourceType,DestType>( uniformInfo, containerOffset, size ) ) );
      }

      template <int n, int m, typename SourceType, typename DestType>
      CacheEntryMatrix<n,m,SourceType, DestType>::CacheEntryMatrix( dp::gl::Program::Uniform const& uniformInfo, size_t containerOffset, size_t size )
        : ParameterCacheEntryStreamBuffer( uniformInfo.offset, containerOffset, 0 )
        , m_arraySize( size )
        , m_arrayStride( uniformInfo.arrayStride )
        , m_matrixStride( uniformInfo.matrixStride )
      {
        DP_ASSERT( (uniformInfo.arraySize == 0 && size == 1) || (uniformInfo.arraySize == dp::checked_cast<GLint>(size)) );

        if ( n > 1 )
        {
          DP_ASSERT( m_matrixStride );
          m_size = n * m_matrixStride;
        }
        else
        {
          m_size = m * sizeof( DestType );
        }
        if ( m_arraySize > 1 )
        {
          DP_ASSERT( m_arrayStride );
          m_size *= m_arrayStride;
        }
      }

      template <int n, int m, typename SourceType, typename DestType>
      void CacheEntryMatrix<n,m,SourceType, DestType>::update( void *cache, void const* container ) const
      {
        SourceType const* containerData = reinterpret_cast<SourceType const*>( reinterpret_cast<const char*>(container) + m_containerOffset );
        for ( size_t arrayIndex = 0; arrayIndex < m_arraySize; ++arrayIndex )
        {
          for (size_t row = 0;row < n;++row )
          {
            DestType* cacheData = reinterpret_cast<DestType*>(reinterpret_cast<char*>(cache) + m_cacheOffset + arrayIndex * m_arrayStride + row * m_matrixStride);
            for (size_t column = 0;column < m; ++column )
            {
              cacheData[column] = *containerData;
              ++containerData;
            }
          }
        }
      }

      ParameterCacheEntryStreamBufferSharedPtr createParameterCacheEntryStreamBuffer( dp::rix::gl::ProgramGLHandle /*program*/
        , dp::rix::core::ContainerParameterType containerParameterType
        , dp::gl::Program::Uniform uniformInfo
        , size_t /*cacheOffset*/
        , size_t containerOffset )
      {
        ParameterCacheEntryStreamBufferSharedPtr parameterCacheEntry;
#if !defined(NDEBUG)
        GLint const& uniformType = uniformInfo.type;
#endif
        size_t newArraySize = 1;
        switch( containerParameterType )
        {
        case dp::rix::core::ContainerParameterType::FLOAT:
          DP_ASSERT( uniformType == GL_FLOAT );
          parameterCacheEntry = CacheEntryMatrix<1, 1, float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::FLOAT2:
          DP_ASSERT( uniformType == GL_FLOAT_VEC2 );
          parameterCacheEntry = CacheEntryMatrix<1, 2, float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::FLOAT3:
          DP_ASSERT( uniformType == GL_FLOAT_VEC3 );
          parameterCacheEntry = CacheEntryMatrix<1, 3, float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::FLOAT4:
          DP_ASSERT( uniformType == GL_FLOAT_VEC4 );
          parameterCacheEntry = CacheEntryMatrix<1, 4, float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::INT_8:
          DP_ASSERT( uniformType == GL_INT );
          parameterCacheEntry = CacheEntryMatrix<1, 1, int8_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT2_8:
          DP_ASSERT( uniformType == GL_INT_VEC2 );
          parameterCacheEntry = CacheEntryMatrix<1, 2, int8_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT3_8:
          DP_ASSERT( uniformType == GL_INT_VEC3 );
          parameterCacheEntry = CacheEntryMatrix<1, 3, int8_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT4_8:
          DP_ASSERT( uniformType == GL_INT_VEC4 );
          parameterCacheEntry = CacheEntryMatrix<1, 4, int8_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::INT_16:
          DP_ASSERT( uniformType == GL_INT );
          parameterCacheEntry = CacheEntryMatrix<1, 1, int16_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT2_16:
          DP_ASSERT( uniformType == GL_INT_VEC2 );
          parameterCacheEntry = CacheEntryMatrix<1, 2, int16_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT3_16:
          DP_ASSERT( uniformType == GL_INT_VEC3 );
          parameterCacheEntry = CacheEntryMatrix<1, 3, int16_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT4_16:
          DP_ASSERT( uniformType == GL_INT_VEC4 );
          parameterCacheEntry = CacheEntryMatrix<1, 4, int16_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::INT_32:
          DP_ASSERT( uniformType == GL_INT );
          parameterCacheEntry = CacheEntryMatrix<1, 1, int32_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT2_32:
          DP_ASSERT( uniformType == GL_INT_VEC2 );
          parameterCacheEntry = CacheEntryMatrix<1, 2, int32_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT3_32:
          DP_ASSERT( uniformType == GL_INT_VEC3 );
          parameterCacheEntry = CacheEntryMatrix<1, 3, int32_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT4_32:
          DP_ASSERT( uniformType == GL_INT_VEC4 );
          parameterCacheEntry = CacheEntryMatrix<1, 4, int32_t, int32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::INT_64:
          DP_ASSERT( uniformType == GL_INT64_NV );
          parameterCacheEntry = CacheEntryMatrix<1, 1, int64_t, int64_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT2_64:
          DP_ASSERT( uniformType == GL_INT64_VEC2_NV );
          parameterCacheEntry = CacheEntryMatrix<1, 2, int64_t, int64_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT3_64:
          DP_ASSERT( uniformType == GL_INT64_VEC3_NV );
          parameterCacheEntry = CacheEntryMatrix<1, 3, int64_t, int64_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::INT4_64:
          DP_ASSERT( uniformType == GL_INT64_VEC4_NV );
          parameterCacheEntry = CacheEntryMatrix<1, 4, int64_t, int64_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::UINT_8:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT );
          parameterCacheEntry = CacheEntryMatrix<1, 1, int8_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT2_8:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC2 );
          parameterCacheEntry = CacheEntryMatrix<1, 2, int8_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT3_8:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC3 );
          parameterCacheEntry = CacheEntryMatrix<1, 3, int8_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT4_8:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC4 );
          parameterCacheEntry = CacheEntryMatrix<1, 4, int8_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::UINT_16:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT );
          parameterCacheEntry = CacheEntryMatrix<1, 1, int16_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT2_16:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC2 );
          parameterCacheEntry = CacheEntryMatrix<1, 2, int16_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT3_16:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC3 );
          parameterCacheEntry = CacheEntryMatrix<1, 3, int16_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT4_16:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC4 );
          parameterCacheEntry = CacheEntryMatrix<1, 4, int16_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::UINT_32:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT );
          parameterCacheEntry = CacheEntryMatrix<1, 1, uint32_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT2_32:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC2 );
          parameterCacheEntry = CacheEntryMatrix<1, 2, uint32_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT3_32:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC3 );
          parameterCacheEntry = CacheEntryMatrix<1, 3, uint32_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT4_32:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT_VEC4 );
          parameterCacheEntry = CacheEntryMatrix<1, 4, uint32_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::UINT_64:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT64_NV );
          parameterCacheEntry = CacheEntryMatrix<1, 1, uint64_t, uint64_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT2_64:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT64_VEC2_NV );
          parameterCacheEntry = CacheEntryMatrix<1, 2, uint64_t, uint64_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT3_64:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT64_VEC3_NV );
          parameterCacheEntry = CacheEntryMatrix<1, 3, uint64_t, uint64_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::UINT4_64:
          DP_ASSERT( uniformType == GL_UNSIGNED_INT64_VEC4_NV );
          parameterCacheEntry = CacheEntryMatrix<1, 4, uint64_t, uint64_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::BOOL:
          DP_ASSERT( uniformType == GL_BOOL );
          parameterCacheEntry = CacheEntryMatrix<1, 1, int8_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::BOOL2:
          DP_ASSERT( uniformType == GL_BOOL_VEC2 );
          parameterCacheEntry = CacheEntryMatrix<1, 2, int8_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::BOOL3:
          DP_ASSERT( uniformType == GL_BOOL_VEC3 );
          parameterCacheEntry = CacheEntryMatrix<1, 3, int8_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::BOOL4:
          DP_ASSERT( uniformType == GL_BOOL_VEC4 );
          parameterCacheEntry = CacheEntryMatrix<1, 4, int8_t, uint32_t>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::MAT2X2:
          DP_ASSERT( uniformType == GL_FLOAT_MAT2 );
          parameterCacheEntry = CacheEntryMatrix<2,2,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::MAT2X3:
          DP_ASSERT( uniformType == GL_FLOAT_MAT2x3 );
          parameterCacheEntry = CacheEntryMatrix<2,3,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::MAT2X4:
          DP_ASSERT( uniformType == GL_FLOAT_MAT2x4 );
          parameterCacheEntry = CacheEntryMatrix<2,4,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::MAT3X2:
          DP_ASSERT( uniformType == GL_FLOAT_MAT3x2 );
          parameterCacheEntry = CacheEntryMatrix<3,2,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::MAT3X3:
          DP_ASSERT( uniformType == GL_FLOAT_MAT3 );
          parameterCacheEntry = CacheEntryMatrix<3,3,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::MAT3X4:
          DP_ASSERT( uniformType == GL_FLOAT_MAT3x4 );
          parameterCacheEntry = CacheEntryMatrix<3,4,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;

        case dp::rix::core::ContainerParameterType::MAT4X2:
          DP_ASSERT( uniformType == GL_FLOAT_MAT4x2 );
          parameterCacheEntry = CacheEntryMatrix<4,2,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::MAT4X3:
          DP_ASSERT( uniformType == GL_FLOAT_MAT4x3 );
          parameterCacheEntry = CacheEntryMatrix<4,3,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        case dp::rix::core::ContainerParameterType::MAT4X4:
          DP_ASSERT( uniformType == GL_FLOAT_MAT4 );
          parameterCacheEntry = CacheEntryMatrix<4,4,float, float>::create( uniformInfo, containerOffset, newArraySize );
          break;
        default:
          DP_ASSERT( !"unknown type" );
        }
        return parameterCacheEntry;
      }

      ParameterCacheEntryStreamBuffers createParameterCacheEntriesStreamBuffer( dp::rix::gl::ProgramGLHandle program
        , dp::rix::gl::ContainerDescriptorGLHandle descriptor
        , dp::rix::gl::ProgramGL::UniformInfos const& uniformInfos )
      {
        std::vector<ParameterCacheEntryStreamBufferSharedPtr> parameterCacheEntries;
        size_t cacheOffset = 0;
        for ( dp::rix::gl::ProgramGL::UniformInfos::const_iterator it = uniformInfos.begin(); it != uniformInfos.end(); ++it )
        {
          size_t parameterIndex = descriptor->getIndex( it->first );
          parameterCacheEntries.push_back( createParameterCacheEntryStreamBuffer( program, descriptor->m_parameterInfos[parameterIndex].m_type, it->second
                                                                                , cacheOffset, descriptor->m_parameterInfos[parameterIndex].m_offset ) );
          cacheOffset += parameterCacheEntries.back()->getSize();
        }
        return parameterCacheEntries;
      }

    } // namespace gl
  } // namespace rix
} // namespace dp
