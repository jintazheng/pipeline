// Copyright (c) 2011-2016, NVIDIA CORPORATION. All rights reserved.
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


#include <dp/sg/xbar/DrawableManager.h>
#include <dp/sg/xbar/SceneTree.h>
#include <dp/sg/xbar/inc/UpdateObjectVisitor.h>
#include <dp/sg/xbar/inc/SceneTreeGenerator.h>

// observers
#include <dp/sg/xbar/inc/ObjectObserver.h>
#include <dp/sg/xbar/inc/SwitchObserver.h>
#include <dp/sg/xbar/inc/TransformObserver.h>
#include <dp/sg/xbar/inc/SceneObserver.h>


#include <dp/math/Boxnt.h>
#include <dp/math/Trafo.h>
#include <dp/sg/core/Group.h>
#include <dp/sg/core/Billboard.h>
#include <dp/sg/core/Camera.h>
#include <dp/sg/core/Group.h>
#include <dp/sg/core/GeoNode.h>
#include <dp/sg/core/LightSource.h>
#include <dp/sg/core/LOD.h>
#include <dp/sg/core/Node.h>
#include <dp/sg/core/Object.h>
#include <dp/sg/core/Primitive.h>
#include <dp/sg/core/Scene.h>
#include <dp/sg/core/Switch.h>
#include <dp/sg/core/Transform.h>

#include <dp/util/FrameProfiler.h>
#include <dp/util/BitArray.h>

#include <algorithm>

using namespace dp::math;
using namespace dp::util;
using namespace dp::sg::core;

using namespace std;

namespace dp
{
  namespace sg
  {

    namespace xbar
    {

      SceneTree::SceneTree( dp::sg::core::SceneSharedPtr const & scene )
        : m_scene( scene )
        , m_rootNode( scene->getRootNode() )
        , m_dirty( false )
        , m_switchObserver( SwitchObserver::create() )
      {
      }

      SceneTree::~SceneTree()
      {
        m_sceneObserver.reset();
      }

      SceneTreeSharedPtr SceneTree::create( SceneSharedPtr const & scene )
      {
        SceneTreeSharedPtr st = std::shared_ptr<SceneTree>( new SceneTree( scene ) );
        st->init();
        return( st );
      }

      void SceneTree::init()
      {
        m_objectObserver = ObjectObserver::create( shared_from_this() );
        m_sceneObserver = SceneObserver::create( shared_from_this() );

        // push a sentinel root group in the vector to avoid special cases for the real root-node later on
        ObjectTreeNode objectTreeSentinel;
        objectTreeSentinel.m_transform = m_transformTree.getTree().getRoot();
        objectTreeSentinel.m_transformParent = -1;
        objectTreeSentinel.m_clipPlaneGroup = ClipPlaneGroup::create();
        m_objectTreeSentinel = m_objectTree.insertNode( objectTreeSentinel, ~0, ~0 );

        SceneTreeGenerator rlg( this->shared_from_this() );
        rlg.setCurrentObjectTreeData( m_objectTreeSentinel, ~0 );
        rlg.apply( m_scene );

        // root node is first child below sentinel
        m_objectTreeRootNode = m_objectTree[m_objectTreeSentinel].m_firstChild;
      }

      dp::sg::core::SceneSharedPtr const & SceneTree::getScene() const
      {
        return m_scene;
      }

      void SceneTree::addSubTree( NodeSharedPtr const& root, ObjectTreeIndex parentIndex, ObjectTreeIndex leftSibling)
      {
        SceneTreeGenerator rlg( this->shared_from_this() );

        rlg.setCurrentObjectTreeData( parentIndex, leftSibling );
        rlg.apply( root );
      }

      void SceneTree::replaceSubTree( NodeSharedPtr const& node, ObjectTreeIndex objectIndex )
      {
        ObjectTreeIndex objectParent = m_objectTree[objectIndex].m_parentIndex;

        DP_ASSERT( objectParent != ~0 );

        // search left ObjectTree sibling
        ObjectTreeIndex objectLeftSibling = ~0;
        ObjectTreeIndex currentIndex = m_objectTree[objectParent].m_firstChild;
        while ( currentIndex != objectIndex )
        {
          objectLeftSibling = currentIndex;
          currentIndex = m_objectTree[currentIndex].m_nextSibling;
        }

        // remove group in both trees
        removeObjectTreeIndex( objectIndex );

        // add group back to tree
        addSubTree( node, objectParent, objectLeftSibling);
      }

      void SceneTree::addRendererOptions( const dp::sg::ui::RendererOptionsSharedPtr& rendererOptions )
      {
      }

      void SceneTree::update(dp::sg::core::CameraSharedPtr const& camera, float lodScaleRange)
      {
        // for now it is important to update the transform tree first to clear the DIRTY_TRANSFORM bit
        {
          dp::util::ProfileEntry p("Update TransformTree");
          updateTransformTree(camera);
        }

        {
          dp::util::ProfileEntry p("Update ObjectTree");
          updateObjectTree(camera, lodScaleRange);
        }
      }

      void SceneTree::updateTransformTree(dp::sg::core::CameraSharedPtr const& camera)
      {
        m_transformTree.compute(camera);
      }

      void SceneTree::updateObjectTree(dp::sg::core::CameraSharedPtr const& camera, float lodRangeScale)
      {
        //
        // first step: update node-local information
        //

        // update dirty object hints & masks
        {
          ObjectObserver::NewCacheData cd;
          m_objectObserver->popNewCacheData( cd );

          ObjectObserver::NewCacheData::const_iterator it, it_end = cd.end();
          for( it=cd.begin(); it!=it_end; ++it )
          {
            ObjectTreeNode& node = m_objectTree[ it->first ];
            node.m_localHints = it->second.m_hints;
            node.m_localMask = it->second.m_mask;

            m_objectTree.markDirty( it->first, ObjectTreeNode::DEFAULT_DIRTY );
          }
        }

        // update dirty switch information
        ObjectTreeIndexSet dirtySwitches;
        m_switchObserver->popDirtySwitches( dirtySwitches );
        if( !dirtySwitches.empty() )
        {
          ObjectTreeIndexSet::iterator it, it_end = dirtySwitches.end();
          for( it=dirtySwitches.begin(); it!=it_end; ++it )
          {
            ObjectTreeIndex index = *it;

            SwitchSharedPtr ssp = m_objectTree.m_switchNodes[ index ].lock();
            DP_ASSERT( ssp );

            ObjectTreeIndex childIndex = m_objectTree[index].m_firstChild;
            // counter for the i-th child
            size_t i = 0;

            while( childIndex != ~0 )
            {
              ObjectTreeNode& childNode = m_objectTree[childIndex];
              DP_ASSERT( childNode.m_parentIndex == index );

              bool newActive = ssp->isActive( dp::checked_cast<unsigned int>(i) );
              if ( childNode.m_localActive != newActive )
              {
                childNode.m_localActive = newActive;
                m_objectTree.markDirty( childIndex, ObjectTreeNode::DEFAULT_DIRTY );
              }

              childIndex = childNode.m_nextSibling;
              ++i;
            }
          }
        }

        // update all lods
        if( !m_objectTree.m_LODs.empty() )
        {
          const Mat44f& worldToView = camera->getWorldToViewMatrix();

          std::map< ObjectTreeIndex, LODWeakPtr >::iterator it, it_end = m_objectTree.m_LODs.end();
          for( it = m_objectTree.m_LODs.begin(); it != it_end; ++it )
          {
            ObjectTreeIndex index = it->first;
            const ObjectTreeNode& node = m_objectTree[ index ];

            Mat44f const & modelToWorld = m_transformTree.getTree().getWorldMatrix(node.m_transform);
            const Mat44f modelToView = modelToWorld * worldToView;
            ObjectTreeIndex activeIndex = it->second.lock()->getLODToUse( modelToView, lodRangeScale );

            ObjectTreeIndex childIndex = m_objectTree[index].m_firstChild;
            // counter for the i-th child
            size_t i = 0;

            while( childIndex != ~0 )
            {
              ObjectTreeNode& childNode = m_objectTree[childIndex];
              DP_ASSERT( childNode.m_parentIndex == index );

              bool newActive = activeIndex == i;
              if ( childNode.m_localActive != newActive )
              {
                childNode.m_localActive = newActive;
                m_objectTree.markDirty( childIndex, ObjectTreeNode::DEFAULT_DIRTY );
              }

              childIndex = childNode.m_nextSibling;
              ++i;
            }
          }
        }

        //
        // second step: update resulting node-world information
        //

        UpdateObjectVisitor objectVisitor( m_objectTree, this );
        PreOrderTreeTraverser<ObjectTree, UpdateObjectVisitor> objectTraverser;

        objectTraverser.processDirtyList( m_objectTree, objectVisitor, ObjectTreeNode::DEFAULT_DIRTY );
        m_objectTree.m_dirtyObjects.clear();
      }

      ObjectTreeIndex SceneTree::addObject( const ObjectTreeNode & node, ObjectTreeIndex parentIndex, ObjectTreeIndex siblingIndex )
      {
        // add object to object tree
        ObjectTreeIndex index = m_objectTree.insertNode( node, parentIndex, siblingIndex );

        // observe object
        m_objectObserver->attach( node.m_object, index );

        ObjectTreeNode const & parentNode = m_objectTree[parentIndex];
        ObjectTreeNode & newNode = m_objectTree[index];
        if (std::dynamic_pointer_cast<dp::sg::core::Transform>(node.m_object))
        {
          newNode.m_transformParent = parentNode.m_transform;
          newNode.m_transform = m_transformTree.addTransform(parentNode.m_transform, std::static_pointer_cast<dp::sg::core::Transform>(node.m_object));
          newNode.m_isTransform = true;
        }
        else if(std::dynamic_pointer_cast<dp::sg::core::Billboard>(node.m_object))
        {
          newNode.m_transformParent = parentNode.m_transform;
          newNode.m_transform = m_transformTree.addBillboard(parentNode.m_transform, std::static_pointer_cast<dp::sg::core::Billboard>(node.m_object));
          newNode.m_isBillboard = true;
        }

        return index;
      }

      void SceneTree::addLOD( LODSharedPtr const& lod, ObjectTreeIndex index )
      {
        DP_ASSERT( m_objectTree.m_LODs.find(index) == m_objectTree.m_LODs.end() );
        m_objectTree.m_LODs[index] = lod;
      }

      void SceneTree::addSwitch( const SwitchSharedPtr& s, ObjectTreeIndex index )
      {
        DP_ASSERT( m_objectTree.m_switchNodes.find(index) == m_objectTree.m_switchNodes.end() );
        m_objectTree.m_switchNodes[index] = s;

        // attach switch observer to switch
        m_switchObserver->attach( s, index );
      }

      void SceneTree::addGeoNode( ObjectTreeIndex index )
      {
        // attach observer
        m_objectTree[index].m_isDrawable = true;
        notify( Event( index, m_objectTree[index], Event::Type::ADDED ) );
      }

      void SceneTree::addLightSource( ObjectTreeIndex index )
      {
        m_lightSources.insert(index);
      }

      void SceneTree::removeObjectTreeIndex( ObjectTreeIndex index )
      {
        // initialize the trafo index for the trafo search with the parent's trafo index
        DP_ASSERT( index != m_objectTreeSentinel && "cannot remove root node" );

        // vector for stack-simulation to eliminate overhead of std::stack
        m_objectIndexStack.resize( m_objectTree.size() );
        size_t begin = 0;
        size_t end   = 0;

        // start traversal at index
        m_objectIndexStack[end] = index;
        ++end;

        while( begin != end )
        {
          ObjectTreeIndex currentIndex = m_objectIndexStack[begin];
          ++begin;
          ObjectTreeNode& current = m_objectTree[currentIndex];

          if ( std::dynamic_pointer_cast<dp::sg::core::LightSource>(m_objectTree[currentIndex].m_object) )
          {
            DP_VERIFY( m_lightSources.erase( currentIndex ) == 1 );
          }

          if ( m_objectTree[currentIndex].m_isDrawable )
          {
            notify( Event( currentIndex, m_objectTree[currentIndex], Event::Type::REMOVED) );
            m_objectTree[index].m_isDrawable = false;
          }

          current.m_clipPlaneGroup.reset();

          // detach current index from object observer
          m_objectObserver->detach( currentIndex );

          // TODO: add observer flag to specify which observers must be detached?
          std::map< ObjectTreeIndex, SwitchWeakPtr >::iterator itSwitch = m_objectTree.m_switchNodes.find( currentIndex );
          if ( itSwitch != m_objectTree.m_switchNodes.end() )
          {
            m_switchObserver->detach( currentIndex );
            m_objectTree.m_switchNodes.erase( itSwitch );
          }

          std::map< ObjectTreeIndex, LODWeakPtr >::iterator itLod = m_objectTree.m_LODs.find( currentIndex );
          if ( itLod != m_objectTree.m_LODs.end() )
          {
            m_objectTree.m_LODs.erase( itLod );
          }

          // check if a transform needs to be removed
          DP_ASSERT( current.m_parentIndex != ~0 );
          const ObjectTreeNode& curParent = m_objectTree[current.m_parentIndex];

          // only remove the topmost transforms below or at index (so transforms are only removed once)
          if (current.m_isTransform)
          {
            m_transformTree.removeTransform(current.m_transform);
          }
          else if (current.m_isBillboard)
          {
            m_transformTree.removeBillboard(current.m_transform);
          }

          current.m_object.reset();

          // insert all children into stack for further traversal
          ObjectTreeIndex child = current.m_firstChild;
          while( child != ~0 )
          {
            m_objectIndexStack[end] = child;
            ++end;
            child = m_objectTree[child].m_nextSibling;
          }
        }

        // delete the node and its children from the object tree
        m_objectTree.deleteNode( index );
      }

      ObjectTree& SceneTree::getObjectTree()
      {
        return m_objectTree;
      }

      ObjectTreeNode& SceneTree::getObjectTreeNode( ObjectTreeIndex index )
      {
        return m_objectTree[index];
      }

      void SceneTree::onRootNodeChanged()
      {
        replaceSubTree( m_scene->getRootNode(), m_objectTreeRootNode );
        m_rootNode = m_scene->getRootNode();
      }

    } // namespace xbar
  } // namespace sg
} // namespace dp
