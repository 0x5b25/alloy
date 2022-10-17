#pragma once

#include <memory>

#include <volk.h>
#include "common/macros.h"

#include "Device.hpp"



class Texture
{
	DISALLOW_COPY_AND_ASSIGN(Texture);

	std::shared_ptr<Device> _dev;

	std::shared_ptr<Buffer> _buf;

	VkImage _img;
	VkImageView _view;
	VkExtent2D _extent;

	enum {EMPTY, ADOPT, SHARE} _memstate = EMPTY;

public:

	Texture() = default;
	~Texture();

	static std::shared_ptr<Texture> Create(
		std::shared_ptr<Device>& device,
		const VkExtent2D& extent
	);

	const VkImage& Handle() const { return _img; }
	const VkImageView& ImageView() const { return _view; }
	const VkExtent2D& Extent() const { return _extent; }

};
