// Copyright NVIDIA Corporation 2012
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


#include <GroupDataUniform.h>

namespace dp
{
  namespace rix
  {
    namespace fx
    {

      GroupDataUniform::GroupDataUniform( ManagerUniform* manager, const SmartParameterGroupSpecInfoHandle& groupSpecInfo)
        : m_groupSpecInfo( groupSpecInfo )
      {
        m_renderer = manager->getRenderer();
        m_container = m_renderer->containerCreate( m_groupSpecInfo->m_descriptor );
      }

      bool GroupDataUniform::setValue( const dp::fx::ParameterGroupSpec::iterator& parameter, const dp::rix::core::ContainerData& data )
      {
        size_t parameterIndex = std::distance( m_groupSpecInfo->m_parameterGroupSpec->beginParameterSpecs(), parameter );
        m_renderer->containerSetData( m_container, m_groupSpecInfo->m_mapping[parameterIndex].m_entry, data );
        return false; // No OpenGL buffer needs to be updated.
      }

      void GroupDataUniform::update( )
      {
      }

    } // namespace fx
  } // namespace rix
} // namespace dp
