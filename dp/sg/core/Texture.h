// Copyright NVIDIA Corporation 2010
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


#pragma once
/** @file */

#include <dp/sg/core/Config.h>
#include <dp/util/HashGeneratorMurMur.h>
#include <dp/sg/core/CoreTypes.h>
#include <dp/sg/core/HandledObject.h>

namespace dp
{
  namespace sg
  {
    namespace core
    {

      //! Texture target specifier
      enum class TextureTarget
      {
        UNSPECIFIED = ~0,     //!< indicates an unspecified target
        TEXTURE_1D = 0,       //!< indicates a 1-dimensional texture
        TEXTURE_2D,           //!< indicates a 2-dimensional texture
        TEXTURE_3D,           //!< indicates a 3-dimensional texture
        TEXTURE_CUBE,         //!< indicates a cube map
        TEXTURE_1D_ARRAY,     //!< indicates a 1d texture array
        TEXTURE_2D_ARRAY,     //!< indicates a 2d texture array
        TEXTURE_RECTANGLE,    //!< indicates a non-power-of-two, rectangular texture
        TEXTURE_CUBE_ARRAY,   //!< indicates a cube map array
        TEXTURE_BUFFER,       //!< indicates a buffer texture
        TEXTURE_TARGET_COUNT  //!< specifies the count of known texture targets
      };

      //! Texture coordinate axis
      enum class TexCoordAxis
      {
        S = 0                 //!< S axis
      , T                     //!< T axis
      , R                     //!< R axis
      , Q                     //!< Q axis
      };

      //! Texture environment mode
      enum class TextureEnvMode
      {
        REPLACE = 0           //!< C = C_t; A = A_t
      , MODULATE              //!< C = C_f*C_t; A = A_f*A_t
      , DECAL                 //!< C = C_f*(1-A_t)+C_t*A_t; A = A_f
      , BLEND                 //!< C = C_f*(1-C_t)+C_c*C_t; A = A_f*A_t
      , ADD                   //!< C = min(1,C_f+C_t); A = A_f*A_t
      , ADD_SIGNED            //!< C = texUnitCur + texUnitPrev - 0.5; A = Ac*Ap
      , SUBTRACT              //!< C = texUnitCur - texUnitPrev; A = Ac*Ap
      , INTERPOLATE           //!< C = tUC * (tevColor) + tUP * (1-tevColor); A = Ac*Ap
      , DOT3_RGB              //!< C = texUnitCur.rgb dot texUnitPrev.rgb; A = N/A
      , DOT3_RGBA             //!< C = texUnitCur.rgba dot texUnitPrev.rgba
      };

      //! Texture environment scale
      enum class TextureEnvScale
      {
        X1 = 1                //!< scale factor 1x or no scaling
      , X2 = 2                //!< scale factor 2x
      , X4 = 4                //!< scale factor 4x
      };

      //! Texture coordinate generation mode
      enum class TexGenMode
      {
        OFF           = 0     //!< Off
      , LINEAR                //!< Object linear
      , EYE_LINEAR            //!< Eye linear
      , SPHERE_MAP            //!< Sphere map
      , REFLECTION_MAP        //!< Reflection map
      , NORMAL_MAP            //!< Normal map
      };

      //! Texture generation plane
      enum class TexGenLinearPlane
      {
        OBJECT = 0            //!< Object
      , EYE                   //!< Eye
      , OBJECT_AND_EYE        //!< Object and eye
      };


      /** \brief Interface for textures used as storage for SceniX. 
       *  \sa TextureHost, TextureGL
      **/
      class Texture : public HandledObject
      {
      public:
        DP_SG_CORE_API virtual ~Texture();

        TextureTarget getTextureTarget() const;

        /*! \brief Increments (increases by one) the mipmap use count
         *  The function increments the  mipmap use count for this Texture image by one.
         *
         *  This function should be called by the client whenever the corresponding minification
         *  filter settings change from a non-mipmap filter mode to a mipmap filter mode. 
         *
         *  The function serves sharing of this TextureHost between multiple clients that
         *  probably have different corresponding minification filter settings. Mipmaps are
         *  required if at least one client has mipmap filtering on.
         *
         *  \sa decrementMipmapUseCount, getMipmapUseCount
         */
        DP_SG_CORE_API void incrementMipmapUseCount();

        /*! \brief Decrements (decreases by one) the mipmap use count
         *  The function decrements the  mipmap use count for this Texture image by one.
         *
         *  This function should be called by the client whenever the corresponding minification
         *  filter settings change from a mipmap filter mode to a non-mipmap filter mode.
         *
         *  If at upload time the mipmap use count is zero, no mipmap generation will be forced, 
         *  nor will existing mipmaps be uploaded to the graphics hardware.
         *
         *  \sa incrementMipmapUseCount, getMipmapUseCount
         */
        DP_SG_CORE_API void decrementMipmapUseCount();

        /*! \brief Returns if any referencing sampler requires mipmaps.
         *  \returns Returns if any referencing sampler requires mipmaps.
         *  Upload code should evaluate this flag for conditional 
         *  mipmap creation and upload. Mipmap creation, if required, can be performed by
         *  calling createMipmaps.
         *  \sa incrementMipmapUseCount, decrementMipmapUseCount, createMipmaps
         */
        DP_SG_CORE_API bool isMipmapRequired() const;

        /*! \brief Get the hash key of this Object.
         *  \return The hash key of this Object.
         *  \remarks If the hash key is not valid, the virtual function feedHashGenerator() is called to
         *  recursively determine a hash string, using a HashGenerator, which then is converted into
         *  a HashKey.
         *  \sa feedHashGenerator */
        DP_SG_CORE_API dp::util::HashKey getHashKey() const;

        /*! \brief Tests whether this Texture is equivalent to another Texture.
          * \param p Pointer to the Texture to test for equivalence with this Texture object.
          * \param deepCompare The function performs a deep-compare instead of a shallow compare if this is \c true.
          * \return The function returns \c true if the Texture pointed to by \a p is detected to be equivalent
          * to this Texture object. */
        DP_SG_CORE_API virtual bool isEquivalent( TextureSharedPtr const& texture, bool deepCompare ) const;


        REFLECTION_INFO_API( DP_SG_CORE_API, Texture );

        BEGIN_DECLARE_STATIC_PROPERTIES
          DP_SG_CORE_API DECLARE_STATIC_PROPERTY( MipmapRequired );
        END_DECLARE_STATIC_PROPERTIES

      protected:
        DP_SG_CORE_API Texture();
        DP_SG_CORE_API Texture( TextureTarget textureTarget );

        DP_SG_CORE_API void setTextureTarget( TextureTarget tt );

        DP_SG_CORE_API virtual void onIncrementMipmapUseCount();
        DP_SG_CORE_API virtual void onDecrementMipmapUseCount();

        /*! \brief Feed the data of this object into the provied HashGenerator.
         *  \param hg The HashGenerator to update with the data of this object.
         *  \sa getHashKey */
        DP_SG_CORE_API virtual void feedHashGenerator( dp::util::HashGenerator & hg ) const;

        /*! \brief Invalidate the HashKey.
         *  \sa getHashKey, feedHashGenerator */
        void invalidateHashKey();

    #if !defined(NDEBUG)
        bool isHashKeyValid() const;
    #endif

      private:
        unsigned int              m_mipmapUseCount;
        TextureTarget             m_textureTarget;

        mutable dp::util::HashKey m_hashKey;
        mutable bool              m_hashKeyValid;
      };

      DP_SG_CORE_API unsigned int textureTargetToType( dp::sg::core::TextureTarget target );
      DP_SG_CORE_API TextureTarget textureTypeToTarget( unsigned int type );

      inline TextureTarget Texture::getTextureTarget() const
      {
        return m_textureTarget;
      }

      inline void Texture::invalidateHashKey()
      {
        m_hashKeyValid = false;
      }

#if !defined(NDEBUG)
      inline bool Texture::isHashKeyValid() const
      {
        return( m_hashKeyValid );
      }
#endif

    } // namespace core
  } // namespace sg
} // namespace dp

