//
// Copyright (c) 2017-2020 the rbfx project.
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

#pragma once

#include "../Core/WorkQueue.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Light.h"
#include "../Graphics/Material.h"
#include "../Graphics/Technique.h"
#include "../Math/NumericRange.h"
#include "../Math/SphericalHarmonics.h"
#include "../RenderPipeline/DrawableLightAccumulator.h"
#include "../RenderPipeline/SceneBatch.h"
#include "../RenderPipeline/SceneBatchCollectorCallback.h"
#include "../RenderPipeline/SceneDrawableData.h"
#include "../RenderPipeline/SceneLight.h"
#include "../RenderPipeline/ScenePass.h"
#include "../RenderPipeline/ScenePipelineStateCache.h"
#include "../RenderPipeline/ShadowMapAllocator.h"

#include <EASTL/fixed_vector.h>
#include <EASTL/sort.h>
#include <EASTL/vector_map.h>

namespace Urho3D
{

/// Utility class to collect batches from the scene for given frame.
class SceneBatchCollector : public Object
{
    URHO3D_OBJECT(SceneBatchCollector, Object);
public:
    /// Max number of vertex lights.
    static const unsigned MaxVertexLights = 4;
    /// Max number of pixel lights. Soft limit, violation leads to performance penalty.
    static const unsigned MaxPixelLights = 4;
    /// Max number of scene passes. Soft limit, violation leads to performance penalty.
    static const unsigned MaxScenePasses = 8;
    /// Collection of vertex lights used (indices).
    using VertexLightCollection = ea::array<unsigned, MaxVertexLights>;

    /// Construct.
    SceneBatchCollector(Context* context);
    /// Destruct.
    ~SceneBatchCollector();

    /// Set max number of pixel lights per drawable. Important lights may override this limit.
    void SetMaxPixelLights(unsigned count) { maxPixelLights_ = count; }
    /// Reset scene passes.
    void ResetPasses();
    /// Set shadow pass.
    void SetShadowPass(const SharedPtr<ShadowScenePass>& shadowPass);
    /// Add scene pass.
    void AddScenePass(const SharedPtr<ScenePass>& pass);

    /// Invalidate pipeline state caches.
    void InvalidatePipelineStateCache();
    /// Begin frame processing.
    void BeginFrame(const FrameInfo& frameInfo, SceneBatchCollectorCallback& callback);
    /// Process visible drawables.
    void ProcessVisibleDrawables(const ea::vector<Drawable*>& drawables);
    /// Process visible lights.
    void ProcessVisibleLights();
    /// Collect scene batches.
    void CollectSceneBatches();
    /// Update geometries.
    void UpdateGeometries();
    /// Collect light volume batches for deferred rendering.
    void CollectLightVolumeBatches();

    /// Return frame info.
    const FrameInfo& GetFrameInfo() const { return frameInfo_; }
    /// Return main light index.
    unsigned GetMainLightIndex() const { return mainLightIndex_; }
    /// Return main light.
    SceneLight* GetMainLight() const { return mainLightIndex_ != M_MAX_UNSIGNED ? visibleLights_[mainLightIndex_] : nullptr; }
    /// Return visible light by index.
    const SceneLight* GetVisibleLight(unsigned i) const { return visibleLights_[i]; }
    /// Return all visible lights.
    const ea::vector<SceneLight*>& GetVisibleLights() const { return visibleLights_; }
    /// Return light volume batches.
    ea::span<const LightVolumeBatch> GetLightVolumeBatches() const { return lightVolumeBatches_; }

    /// Return vertex lights for drawable (as indices in the array of visible lights).
    VertexLightCollection GetVertexLightIndices(unsigned drawableIndex) const { return drawableLighting_[drawableIndex].GetVertexLights(); }
    /// Return vertex lights for drawable (as pointers).
    ea::array<SceneLight*, MaxVertexLights> GetVertexLights(unsigned drawableIndex) const;

private:
    /// Update source batches and collect pass batches for single thread.
    void ProcessVisibleDrawablesForThread(unsigned threadIndex, ea::span<Drawable* const> drawables);

    /// Find main light.
    unsigned FindMainLight() const;
    /// Accumulate forward lighting for given light.
    void AccumulateForwardLighting(unsigned lightIndex);

    /// Max number of pixel lights per drawable. Important lights may override this limit.
    unsigned maxPixelLights_{ 1 };

    /// Min number of processed drawables in single task.
    unsigned drawableWorkThreshold_{ 1 };
    /// Min number of processed lit geometries in single task.
    unsigned litGeometriesWorkThreshold_{ 1 };
    /// Min number of processed batches in single task.
    unsigned batchWorkThreshold_{ 1 };

    /// Work queue.
    WorkQueue* workQueue_{};
    /// Renderer.
    Renderer* renderer_{};
    /// Pipeline state factory.
    SceneBatchCollectorCallback* callback_{};
    /// Number of worker threads.
    unsigned numThreads_{};
    /// Material quality.
    MaterialQuality materialQuality_{};

    /// Frame info.
    FrameInfo frameInfo_;
    /// Octree.
    Octree* octree_{};
    /// Camera.
    Camera* camera_{};
    /// Number of drawables.
    unsigned numDrawables_{};

    /// Shadow pass.
    SharedPtr<ShadowScenePass> shadowPass_;
    /// Scene passes.
    /// TODO(renderer): Rename
    ea::vector<SharedPtr<ScenePass>> passes2_;

    /// Visible geometries.
    WorkQueueVector<Drawable*> visibleGeometries_;
    /// Temporary thread-safe collection of visible lights.
    WorkQueueVector<Light*> visibleLightsTemp_;
    /// Visible lights.
    ea::vector<SceneLight*> visibleLights_;
    /// Index of main directional light in visible lights collection.
    unsigned mainLightIndex_{ M_MAX_UNSIGNED };
    /// Scene Z range.
    SceneZRange sceneZRange_;

    /// Shadow caster drawables to be updated.
    WorkQueueVector<Drawable*> shadowCastersToBeUpdated_;
    /// Geometries to be updated from worker threads.
    WorkQueueVector<Drawable*> threadedGeometryUpdates_;
    /// Geometries to be updated from main thread.
    WorkQueueVector<Drawable*> nonThreadedGeometryUpdates_;

    /// Common drawable data index.
    SceneDrawableData transient_;
    /// Drawable lighting data index.
    ea::vector<DrawableLightAccumulator<MaxPixelLights, MaxVertexLights>> drawableLighting_;
    /// Light volume batches.
    ea::vector<LightVolumeBatch> lightVolumeBatches_;

    /// Cached lights data.
    ea::unordered_map<WeakPtr<Light>, ea::unique_ptr<SceneLight>> cachedSceneLights_;
};

}
