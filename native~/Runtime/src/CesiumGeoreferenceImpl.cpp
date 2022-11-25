#include "CesiumGeoreferenceImpl.h"

#include "UnityTransforms.h"

#include <CesiumGeospatial/LocalHorizontalCoordinateSystem.h>
#include <CesiumUtility/Math.h>

#include <DotNet/CesiumForUnity/CesiumGeoreference.h>
#include <DotNet/CesiumForUnity/CesiumGeoreferenceOriginAuthority.h>
#include <DotNet/CesiumForUnity/CesiumGlobeAnchor.h>
#include <DotNet/CesiumForUnity/CesiumGlobeAnchorPositionAuthority.h>
#include <DotNet/System/Array1.h>
#include <DotNet/UnityEngine/GameObject.h>
#include <DotNet/UnityEngine/Matrix4x4.h>
#include <DotNet/UnityEngine/Quaternion.h>
#include <DotNet/UnityEngine/Transform.h>
#include <DotNet/UnityEngine/Vector3.h>

using namespace CesiumForUnityNative;
using namespace CesiumGeospatial;
using namespace CesiumUtility;

namespace {

LocalHorizontalCoordinateSystem createCoordinateSystem(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference) {
  if (georeference.originAuthority() ==
      DotNet::CesiumForUnity::CesiumGeoreferenceOriginAuthority::
          LongitudeLatitudeHeight) {
    return LocalHorizontalCoordinateSystem(
        Cartographic::fromDegrees(
            georeference.longitude(),
            georeference.latitude(),
            georeference.height()),
        LocalDirection::East,
        LocalDirection::Up,
        LocalDirection::North);
  } else {
    return LocalHorizontalCoordinateSystem(
        glm::dvec3(
            georeference.ecefX(),
            georeference.ecefY(),
            georeference.ecefZ()),
        LocalDirection::East,
        LocalDirection::Up,
        LocalDirection::North);
  }
}

} // namespace

CesiumGeoreferenceImpl::CesiumGeoreferenceImpl(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference)
    : _coordinateSystem(createCoordinateSystem(georeference)) {}

CesiumGeoreferenceImpl::~CesiumGeoreferenceImpl() {}

void CesiumGeoreferenceImpl::JustBeforeDelete(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference) {}

void CesiumGeoreferenceImpl::RecalculateOrigin(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference) {
  LocalHorizontalCoordinateSystem coordinateSystem =
      createCoordinateSystem(georeference);

  if (coordinateSystem.getLocalToEcefTransformation() ==
      this->_coordinateSystem.getLocalToEcefTransformation()) {
    // No change
    return;
  }

  // Update all globe anchors based on the new origin.
  std::swap(this->_coordinateSystem, coordinateSystem);

  glm::dmat3 oldToEcef =
      glm::dmat3(coordinateSystem.getLocalToEcefTransformation());
  glm::dmat3 ecefToNew =
      glm::dmat3(this->_coordinateSystem.getEcefToLocalTransformation());
  glm::dmat3 oldToNew = ecefToNew * oldToEcef;

  DotNet::System::Array1<DotNet::CesiumForUnity::CesiumGlobeAnchor> anchors =
      georeference.gameObject()
          .GetComponentsInChildren<DotNet::CesiumForUnity::CesiumGlobeAnchor>(
              true);

  for (int32_t i = 0; i < anchors.Length(); ++i) {
    DotNet::CesiumForUnity::CesiumGlobeAnchor anchor = anchors[i];

    DotNet::UnityEngine::Transform transform = anchor.transform();
    glm::dmat3 modelToOld =
        glm::dmat3(UnityTransforms::fromUnity(transform.localToWorldMatrix()));
    glm::dmat3 modelToNew = oldToNew * modelToOld;
    RotationAndScale rotationAndScale =
        UnityTransforms::matrixToRotationAndScale(modelToNew);

    transform.rotation(UnityTransforms::toUnity(rotationAndScale.rotation));
    transform.localScale(UnityTransforms::toUnity(rotationAndScale.scale));

    // The meaning of Unity coordinates will change with the georeference
    // change, so switch to ECEF if necessary.
    DotNet::CesiumForUnity::CesiumGlobeAnchorPositionAuthority authority =
        anchor.positionAuthority();
    if (authority ==
        DotNet::CesiumForUnity::CesiumGlobeAnchorPositionAuthority::
            UnityWorldCoordinates) {
      authority = DotNet::CesiumForUnity::CesiumGlobeAnchorPositionAuthority::
          EarthCenteredEarthFixed;
    }

    // Re-assign the (probably unchanged) authority to recompute Unity
    // coordinates with the new georeference.
    anchor.positionAuthority(authority);
  }
}

void CesiumGeoreferenceImpl::InitializeOrigin(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference) {
  // Compute the initial coordinate system. Don't call RecalculateOrigin because
  // that will also rotate objects based on the new origin.
  this->_coordinateSystem = createCoordinateSystem(georeference);
}

DotNet::Unity::Mathematics::double3
CesiumGeoreferenceImpl::TransformUnityWorldPositionToEarthCenteredEarthFixed(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference,
    DotNet::Unity::Mathematics::double3 unityWorldPosition) {
  const LocalHorizontalCoordinateSystem& coordinateSystem =
      this->getCoordinateSystem();
  glm::dvec3 result = coordinateSystem.localPositionToEcef(glm::dvec3(
      unityWorldPosition.x,
      unityWorldPosition.y,
      unityWorldPosition.z));
  return DotNet::Unity::Mathematics::double3{result.x, result.y, result.z};
}

DotNet::Unity::Mathematics::double3
CesiumGeoreferenceImpl::TransformUnityLocalPositionToEarthCenteredEarthFixed(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference,
    const DotNet::UnityEngine::Transform& parent,
    DotNet::Unity::Mathematics::double3 unityLocalPosition) {
  if (parent != nullptr) {
    // Transform the local position to a world position.
    // TODO: we could achieve better position by composing the transform chain
    // ourselves rather than letting Unity do it in single precision.
    glm::dvec4 position =
        UnityTransforms::fromUnity(parent.localToWorldMatrix()) *
        glm::dvec4(
            unityLocalPosition.x,
            unityLocalPosition.y,
            unityLocalPosition.z,
            1.0);

    // Transform the world position to ECEF
    return this->TransformUnityWorldPositionToEarthCenteredEarthFixed(
        georeference,
        DotNet::Unity::Mathematics::double3{
            position.x,
            position.y,
            position.z});
  } else {
    // Local and world coordinates are equivalent
    return this->TransformUnityWorldPositionToEarthCenteredEarthFixed(
        georeference,
        unityLocalPosition);
  }
}

DotNet::Unity::Mathematics::double3
CesiumGeoreferenceImpl::TransformEarthCenteredEarthFixedPositionToUnityWorld(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference,
    DotNet::Unity::Mathematics::double3 earthCenteredEarthFixed) {
  const LocalHorizontalCoordinateSystem& coordinateSystem =
      this->getCoordinateSystem();
  glm::dvec3 result = coordinateSystem.ecefPositionToLocal(glm::dvec3(
      earthCenteredEarthFixed.x,
      earthCenteredEarthFixed.y,
      earthCenteredEarthFixed.z));
  return DotNet::Unity::Mathematics::double3{result.x, result.y, result.z};
}

DotNet::Unity::Mathematics::double3
CesiumGeoreferenceImpl::TransformEarthCenteredEarthFixedPositionToUnityLocal(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference,
    const DotNet::UnityEngine::Transform& parent,
    DotNet::Unity::Mathematics::double3 earthCenteredEarthFixed) {
  // Transform ECEF to Unity world
  DotNet::Unity::Mathematics::double3 worldPosition =
      this->TransformEarthCenteredEarthFixedPositionToUnityWorld(
          georeference,
          earthCenteredEarthFixed);
  if (parent == nullptr) {
    // World and Local are equivalent
    return worldPosition;
  } else {
    // Transform Unity World to Unity Local
    glm::dvec4 position =
        UnityTransforms::fromUnity(parent.worldToLocalMatrix()) *
        glm::dvec4(worldPosition.x, worldPosition.y, worldPosition.z, 1.0);
    return DotNet::Unity::Mathematics::double3{
        position.x,
        position.y,
        position.z};
  }
}

DotNet::Unity::Mathematics::double3
CesiumGeoreferenceImpl::TransformUnityWorldDirectionToEarthCenteredEarthFixed(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference,
    DotNet::Unity::Mathematics::double3 unityWorldDirection) {
  const LocalHorizontalCoordinateSystem& coordinateSystem =
      this->getCoordinateSystem();
  glm::dvec3 result = coordinateSystem.localDirectionToEcef(glm::dvec3(
      unityWorldDirection.x,
      unityWorldDirection.y,
      unityWorldDirection.z));
  return DotNet::Unity::Mathematics::double3{result.x, result.y, result.z};
}

DotNet::Unity::Mathematics::double3
CesiumGeoreferenceImpl::TransformEarthCenteredEarthFixedDirectionToUnityWorld(
    const DotNet::CesiumForUnity::CesiumGeoreference& georeference,
    DotNet::Unity::Mathematics::double3 earthCenteredEarthFixedDirection) {
  const LocalHorizontalCoordinateSystem& coordinateSystem =
      this->getCoordinateSystem();
  glm::dvec3 result = coordinateSystem.ecefDirectionToLocal(glm::dvec3(
      earthCenteredEarthFixedDirection.x,
      earthCenteredEarthFixedDirection.y,
      earthCenteredEarthFixedDirection.z));
  return DotNet::Unity::Mathematics::double3{result.x, result.y, result.z};
}
