#include "veldrid/DeviceResource.hpp"
#include "veldrid/GraphicsDevice.hpp"

namespace Veldrid {

	DeviceResource::DeviceResource(const sp<GraphicsDevice>& dev)
		: dev(dev)
	{
	}

	DeviceResource::~DeviceResource() {

	}


	std::string DeviceResource::GetDebugName() const {
		return "<DEV_RESOURCE_BASE>";
	}

}
