/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Binding shader access tests
 *//*--------------------------------------------------------------------*/

#include "vktBindingShaderAccessTests.hpp"

#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"

#include "qpInfo.h"

namespace vkt
{
namespace BindingModel
{
namespace
{

enum ResourceFlag
{
	RESOURCE_FLAG_IMMUTABLE_SAMPLER = (1u << 0u),

	RESOURCE_FLAG_LAST				= (1u << 1u)
};

static const char* const s_quadrantGenVertexPosSource =	"	highp int quadPhase = gl_VertexID % 6;\n"
														"	highp int quadXcoord = int(quadPhase == 1 || quadPhase == 4 || quadPhase == 5);\n"
														"	highp int quadYcoord = int(quadPhase == 2 || quadPhase == 3 || quadPhase == 5);\n"
														"	highp int quadOriginX = (gl_VertexID / 6) % 2;\n"
														"	highp int quadOriginY = (gl_VertexID / 6) / 2;\n"
														"	quadrant_id = gl_VertexID / 6;\n"
														"	result_position = vec4(float(quadOriginX + quadXcoord - 1), float(quadOriginY + quadYcoord - 1), 0.0, 1.0);\n";

bool isUniformDescriptorType (vk::VkDescriptorType type)
{
	return type == vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
		   type == vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
		   type == vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
}

bool isDynamicDescriptorType (vk::VkDescriptorType type)
{
	return type == vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || type == vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
}

vk::VkFormat mapToVkTextureFormat (const tcu::TextureFormat& format)
{
	if (format == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
		return vk::VK_FORMAT_R8G8B8A8_UNORM;

	DE_FATAL("Not implemented");
	return vk::VK_FORMAT_UNDEFINED;
}

vk::VkImageType viewTypeToImageType (vk::VkImageViewType type)
{
	switch (type)
	{
		case vk::VK_IMAGE_VIEW_TYPE_1D:
		case vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY:	return vk::VK_IMAGE_TYPE_1D;
		case vk::VK_IMAGE_VIEW_TYPE_2D:
		case vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY:	return vk::VK_IMAGE_TYPE_2D;
		case vk::VK_IMAGE_VIEW_TYPE_3D:			return vk::VK_IMAGE_TYPE_3D;
		case vk::VK_IMAGE_VIEW_TYPE_CUBE:
		case vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:	return vk::VK_IMAGE_TYPE_2D;

		default:
			DE_FATAL("Impossible");
			return (vk::VkImageType)0;
	}
}

vk::VkTexFilter mapMagFilterToVkTexFilter (tcu::Sampler::FilterMode mode)
{
	switch (mode)
	{
		case tcu::Sampler::NEAREST:	return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR:	return vk::VK_TEX_FILTER_LINEAR;

		default:
			DE_FATAL("Illegal filter mode");
			return (vk::VkTexFilter)0;
	}
}

vk::VkTexFilter mapMinFilterToVkTexFilter (tcu::Sampler::FilterMode mode)
{
	switch (mode)
	{
		case tcu::Sampler::NEAREST:					return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR:					return vk::VK_TEX_FILTER_LINEAR;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return vk::VK_TEX_FILTER_LINEAR;
		case tcu::Sampler::NEAREST_MIPMAP_LINEAR:	return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return vk::VK_TEX_FILTER_LINEAR;

		default:
			DE_FATAL("Illegal filter mode");
			return (vk::VkTexFilter)0;
	}
}

vk::VkTexMipmapMode mapMinFilterToVkTexMipmapMode (tcu::Sampler::FilterMode mode)
{
	switch (mode)
	{
		case tcu::Sampler::NEAREST:					return vk::VK_TEX_MIPMAP_MODE_BASE;
		case tcu::Sampler::LINEAR:					return vk::VK_TEX_MIPMAP_MODE_BASE;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return vk::VK_TEX_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return vk::VK_TEX_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::NEAREST_MIPMAP_LINEAR:	return vk::VK_TEX_MIPMAP_MODE_LINEAR;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return vk::VK_TEX_MIPMAP_MODE_LINEAR;

		default:
			DE_FATAL("Illegal filter mode");
			return (vk::VkTexMipmapMode)0;
	}
}

vk::VkTexAddressMode mapToVkTexAddressMode (tcu::Sampler::WrapMode mode)
{
	switch (mode)
	{
		case tcu::Sampler::CLAMP_TO_EDGE:		return vk::VK_TEX_ADDRESS_MODE_CLAMP;
		case tcu::Sampler::CLAMP_TO_BORDER:		return vk::VK_TEX_ADDRESS_MODE_CLAMP_BORDER;
		case tcu::Sampler::REPEAT_GL:			return vk::VK_TEX_ADDRESS_MODE_WRAP;
		case tcu::Sampler::MIRRORED_REPEAT_GL:	return vk::VK_TEX_ADDRESS_MODE_MIRROR;

		default:
			DE_FATAL("Illegal wrap mode");
			return (vk::VkTexAddressMode)0;
	}
}

vk::VkCompareOp mapToVkCompareOp (tcu::Sampler::CompareMode mode)
{
	switch (mode)
	{
		case tcu::Sampler::COMPAREMODE_LESS:				return vk::VK_COMPARE_OP_LESS;
		case tcu::Sampler::COMPAREMODE_LESS_OR_EQUAL:		return vk::VK_COMPARE_OP_LESS_EQUAL;
		case tcu::Sampler::COMPAREMODE_GREATER:				return vk::VK_COMPARE_OP_GREATER;
		case tcu::Sampler::COMPAREMODE_GREATER_OR_EQUAL:	return vk::VK_COMPARE_OP_GREATER_EQUAL;
		case tcu::Sampler::COMPAREMODE_EQUAL:				return vk::VK_COMPARE_OP_EQUAL;
		case tcu::Sampler::COMPAREMODE_NOT_EQUAL:			return vk::VK_COMPARE_OP_NOT_EQUAL;
		case tcu::Sampler::COMPAREMODE_ALWAYS:				return vk::VK_COMPARE_OP_ALWAYS;
		case tcu::Sampler::COMPAREMODE_NEVER:				return vk::VK_COMPARE_OP_NEVER;

		default:
			DE_FATAL("Illegal compare mode");
			return (vk::VkCompareOp)0;
	}
}

deUint32 getTextureLevelPyramidDataSize (const tcu::TextureLevelPyramid& srcImage)
{
	deUint32 dataSize = 0;
	for (int level = 0; level < srcImage.getNumLevels(); ++level)
	{
		const tcu::ConstPixelBufferAccess srcAccess = srcImage.getLevel(level);

		// tightly packed
		DE_ASSERT(srcAccess.getFormat().getPixelSize() == srcAccess.getPixelPitch());

		dataSize += srcAccess.getWidth() * srcAccess.getHeight() * srcAccess.getDepth() * srcAccess.getFormat().getPixelSize();
	}
	return dataSize;
}

void writeTextureLevelPyramidData (void* dst, deUint32 dstLen, const tcu::TextureLevelPyramid& srcImage, vk::VkImageViewType viewType, std::vector<vk::VkBufferImageCopy>* copySlices)
{
	// \note cube is copied face-by-face
	const deUint32	arraySize	= (viewType == vk::VK_IMAGE_VIEW_TYPE_1D || viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)		? (srcImage.getLevel(0).getHeight()) :
								  (viewType == vk::VK_IMAGE_VIEW_TYPE_2D || viewType == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY)		? (srcImage.getLevel(0).getDepth()) :
								  (viewType == vk::VK_IMAGE_VIEW_TYPE_3D)														? (1) :
								  (viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE || viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)	? (srcImage.getLevel(0).getDepth()) :
								  ((deUint32)0);
	deUint32		levelOffset	= 0;

	DE_ASSERT(arraySize != 0);

	for (int level = 0; level < srcImage.getNumLevels(); ++level)
	{
		const tcu::ConstPixelBufferAccess	srcAccess		= srcImage.getLevel(level);
		const tcu::PixelBufferAccess		dstAccess		(srcAccess.getFormat(), srcAccess.getSize(), srcAccess.getPitch(), (deUint8*)dst + levelOffset);
		const deUint32						dataSize		= srcAccess.getWidth() * srcAccess.getHeight() * srcAccess.getDepth() * srcAccess.getFormat().getPixelSize();
		const deUint32						sliceDataSize	= dataSize / arraySize;
		const deInt32						sliceHeight		= (viewType == vk::VK_IMAGE_VIEW_TYPE_1D || viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY) ? (1) : (srcAccess.getHeight());
		const deInt32						sliceDepth		= (viewType == vk::VK_IMAGE_VIEW_TYPE_3D) ? (srcAccess.getDepth()) : (1);
		const tcu::IVec3					sliceSize		(srcAccess.getWidth(), sliceHeight, sliceDepth);

		// tightly packed
		DE_ASSERT(srcAccess.getFormat().getPixelSize() == srcAccess.getPixelPitch());

		for (int sliceNdx = 0; sliceNdx < (int)arraySize; ++sliceNdx)
		{
			const vk::VkBufferImageCopy copySlice =
			{
				(vk::VkDeviceSize)levelOffset + sliceNdx * sliceDataSize,	// bufferOffset
				(deUint32)sliceSize.x(),									// bufferRowLength
				(deUint32)sliceSize.y(),									// bufferImageHeight
				{
					vk::VK_IMAGE_ASPECT_COLOR,			// aspect
					(deUint32)level,					// mipLevel
					(deUint32)sliceNdx,					// arrayLayer
					1u,									// arraySize
				},															// imageSubresource
				{
					0,
					0,
					0,
				},															// imageOffset
				{
					sliceSize.x(),
					sliceSize.y(),
					sliceSize.z(),
				}															// imageExtent
			};
			copySlices->push_back(copySlice);
		}

		DE_ASSERT(arraySize * sliceDataSize == dataSize);

		tcu::copy(dstAccess, srcAccess);
		levelOffset += dataSize;
	}

	DE_ASSERT(dstLen == levelOffset);
	DE_UNREF(dstLen);
}

de::MovePtr<vk::Allocation> allocateAndBindObjectMemory (const vk::DeviceInterface& vki, vk::VkDevice device, vk::Allocator& allocator, vk::VkBuffer buffer, vk::MemoryRequirement requirement)
{
	const vk::VkMemoryRequirements	requirements	= vk::getBufferMemoryRequirements(vki, device, buffer);
	de::MovePtr<vk::Allocation>		allocation		= allocator.allocate(requirements, requirement);

	VK_CHECK(vki.bindBufferMemory(device, buffer, allocation->getMemory(), allocation->getOffset()));
	return allocation;
}

de::MovePtr<vk::Allocation> allocateAndBindObjectMemory (const vk::DeviceInterface& vki, vk::VkDevice device, vk::Allocator& allocator, vk::VkImage image, vk::MemoryRequirement requirement)
{
	const vk::VkMemoryRequirements	requirements	= vk::getImageMemoryRequirements(vki, device, image);
	de::MovePtr<vk::Allocation>		allocation		= allocator.allocate(requirements, requirement);

	VK_CHECK(vki.bindImageMemory(device, image, allocation->getMemory(), allocation->getOffset()));
	return allocation;
}

vk::VkDescriptorInfo createDescriptorInfo (vk::VkBuffer buffer, vk::VkDeviceSize offset, vk::VkDeviceSize range)
{
	const vk::VkDescriptorInfo resultInfo =
	{
		0,							// bufferView
		0,							// sampler
		0,							// imageView
		(vk::VkImageLayout)0,		// imageLayout
		{ buffer, offset, range }	// bufferInfo
	};
	return resultInfo;
}

vk::VkDescriptorInfo createDescriptorInfo (vk::VkBufferView bufferView)
{
	const vk::VkDescriptorInfo resultInfo =
	{
		bufferView,					// bufferView
		0,							// sampler
		0,							// imageView
		(vk::VkImageLayout)0,		// imageLayout
		{ (vk::VkBuffer)0, 0, 0 }	// bufferInfo
	};
	return resultInfo;
}

vk::VkDescriptorInfo createDescriptorInfo (vk::VkSampler sampler)
{
	const vk::VkDescriptorInfo resultInfo =
	{
		0,							// bufferView
		sampler,					// sampler
		0,							// imageView
		(vk::VkImageLayout)0,		// imageLayout
		{ (vk::VkBuffer)0, 0, 0 }	// bufferInfo
	};
	return resultInfo;
}

vk::VkDescriptorInfo createDescriptorInfo (vk::VkImageView imageView, vk::VkImageLayout layout)
{
	const vk::VkDescriptorInfo resultInfo =
	{
		0,							// bufferView
		0,							// sampler
		imageView,					// imageView
		layout,						// imageLayout
		{ (vk::VkBuffer)0, 0, 0 }	// bufferInfo
	};
	return resultInfo;
}

vk::VkDescriptorInfo createDescriptorInfo (vk::VkSampler sampler, vk::VkImageView imageView, vk::VkImageLayout layout)
{
	const vk::VkDescriptorInfo resultInfo =
	{
		0,							// bufferView
		sampler,					// sampler
		imageView,					// imageView
		layout,						// imageLayout
		{ (vk::VkBuffer)0, 0, 0 }	// bufferInfo
	};
	return resultInfo;
}

vk::VkClearValue createClearValueColor (const tcu::Vec4& color)
{
	vk::VkClearValue retVal;

	retVal.color.float32[0] = color.x();
	retVal.color.float32[1] = color.y();
	retVal.color.float32[2] = color.z();
	retVal.color.float32[3] = color.w();

	return retVal;
};

void drawQuadrantReferenceResult (const tcu::PixelBufferAccess& dst, const tcu::Vec4& c1, const tcu::Vec4& c2, const tcu::Vec4& c3, const tcu::Vec4& c4)
{
	tcu::clear(tcu::getSubregion(dst, 0,					0,						dst.getWidth() / 2,						dst.getHeight() / 2),					c1);
	tcu::clear(tcu::getSubregion(dst, dst.getWidth() / 2,	0,						dst.getWidth() - dst.getWidth() / 2,	dst.getHeight() / 2),					c2);
	tcu::clear(tcu::getSubregion(dst, 0,					dst.getHeight() / 2,	dst.getWidth() / 2,						dst.getHeight() - dst.getHeight() / 2),	c3);
	tcu::clear(tcu::getSubregion(dst, dst.getWidth() / 2,	dst.getHeight() / 2,	dst.getWidth() - dst.getWidth() / 2,	dst.getHeight() - dst.getHeight() / 2),	c4);
}

class SingleTargetRenderInstance : public vkt::TestInstance
{
public:
											SingleTargetRenderInstance	(Context&			context,
																		 const tcu::UVec2&	size);

private:
	static vk::Move<vk::VkImage>			createColorAttachment		(const vk::DeviceInterface&		vki,
																		 vk::VkDevice					device,
																		 vk::Allocator&					allocator,
																		 const tcu::TextureFormat&		format,
																		 const tcu::UVec2&				size,
																		 de::MovePtr<vk::Allocation>*	outAllocation);

	static vk::Move<vk::VkImageView>		createColorAttachmentView	(const vk::DeviceInterface&	vki,
																		 vk::VkDevice				device,
																		 const tcu::TextureFormat&	format,
																		 vk::VkImage				image);

	static vk::Move<vk::VkRenderPass>		createRenderPass			(const vk::DeviceInterface&	vki,
																		 vk::VkDevice				device,
																		 const tcu::TextureFormat&	format);

	static vk::Move<vk::VkFramebuffer>		createFramebuffer			(const vk::DeviceInterface&	vki,
																		 vk::VkDevice				device,
																		 vk::VkRenderPass			renderpass,
																		 vk::VkImageView			colorAttachmentView,
																		 const tcu::UVec2&			size);

	static vk::Move<vk::VkCmdPool>			createCommandPool			(const vk::DeviceInterface&	vki,
																		 vk::VkDevice				device,
																		 deUint32					queueFamilyIndex);

	virtual void							logTestPlan					(void) const = 0;
	virtual void							renderToTarget				(void) = 0;
	virtual tcu::TestStatus					verifyResultImage			(const tcu::ConstPixelBufferAccess& result) const = 0;

	void									readRenderTarget			(tcu::TextureLevel& dst);
	tcu::TestStatus							iterate						(void);

protected:
	const tcu::TextureFormat				m_targetFormat;
	const tcu::UVec2						m_targetSize;

	const vk::DeviceInterface&				m_vki;
	const vk::VkDevice						m_device;
	const vk::VkQueue						m_queue;
	const deUint32							m_queueFamilyIndex;
	vk::Allocator&							m_allocator;
	de::MovePtr<vk::Allocation>				m_colorAttachmentMemory;
	const vk::Unique<vk::VkImage>			m_colorAttachmentImage;
	const vk::Unique<vk::VkImageView>		m_colorAttachmentView;
	const vk::Unique<vk::VkRenderPass>		m_renderPass;
	const vk::Unique<vk::VkFramebuffer>		m_framebuffer;
	const vk::Unique<vk::VkCmdPool>			m_cmdPool;

	bool									m_firstIteration;
};

SingleTargetRenderInstance::SingleTargetRenderInstance (Context&			context,
														const tcu::UVec2&	size)
	: vkt::TestInstance			(context)
	, m_targetFormat			(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)
	, m_targetSize				(size)
	, m_vki						(context.getDeviceInterface())
	, m_device					(context.getDevice())
	, m_queue					(context.getUniversalQueue())
	, m_queueFamilyIndex		(context.getUniversalQueueFamilyIndex())
	, m_allocator				(context.getDefaultAllocator())
	, m_colorAttachmentMemory	(DE_NULL)
	, m_colorAttachmentImage	(createColorAttachment(m_vki, m_device, m_allocator, m_targetFormat, m_targetSize, &m_colorAttachmentMemory))
	, m_colorAttachmentView		(createColorAttachmentView(m_vki, m_device, m_targetFormat, *m_colorAttachmentImage))
	, m_renderPass				(createRenderPass(m_vki, m_device, m_targetFormat))
	, m_framebuffer				(createFramebuffer(m_vki, m_device, *m_renderPass, *m_colorAttachmentView, m_targetSize))
	, m_cmdPool					(createCommandPool(m_vki, m_device, context.getUniversalQueueFamilyIndex()))
	, m_firstIteration			(true)
{
}

vk::Move<vk::VkImage> SingleTargetRenderInstance::createColorAttachment (const vk::DeviceInterface&		vki,
																		 vk::VkDevice					device,
																		 vk::Allocator&					allocator,
																		 const tcu::TextureFormat&		format,
																		 const tcu::UVec2&				size,
																		 de::MovePtr<vk::Allocation>*	outAllocation)
{
	const vk::VkImageCreateInfo	imageInfo	=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		vk::VK_IMAGE_TYPE_2D,							// imageType
		mapToVkTextureFormat(format),					// format
		{ (deInt32)size.x(), (deInt32)size.y(), 1 },	// extent
		1,												// mipLevels
		1,												// arraySize
		1,												// samples
		vk::VK_IMAGE_TILING_OPTIMAL,					// tiling
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT,	// usage
		0,												// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,					// sharingMode
		0u,												// queueFamilyCount
		DE_NULL,										// pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					// initialLayout
	};

	vk::Move<vk::VkImage>		image		(vk::createImage(vki, device, &imageInfo));
	de::MovePtr<vk::Allocation>	allocation	(allocateAndBindObjectMemory(vki, device, allocator, *image, vk::MemoryRequirement::Any));

	*outAllocation = allocation;
	return image;
}

vk::Move<vk::VkImageView> SingleTargetRenderInstance::createColorAttachmentView (const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 const tcu::TextureFormat&	format,
																				 vk::VkImage				image)
{
	const vk::VkImageViewCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		image,							// image
		vk::VK_IMAGE_VIEW_TYPE_2D,		// viewType
		mapToVkTextureFormat(format),	// format
		{ vk::VK_CHANNEL_SWIZZLE_R, vk::VK_CHANNEL_SWIZZLE_G, vk::VK_CHANNEL_SWIZZLE_B, vk::VK_CHANNEL_SWIZZLE_A },
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,	// aspectMask
			0u,								// baseMipLevel
			1u,								// mipLevels
			0u,								// baseArrayLayer
			1u,								// arraySize
		},
		0u,								// flags
	};

	return vk::createImageView(vki, device, &createInfo);
}

vk::Move<vk::VkRenderPass> SingleTargetRenderInstance::createRenderPass (const vk::DeviceInterface&		vki,
																		 vk::VkDevice					device,
																		 const tcu::TextureFormat&		format)
{
	const vk::VkAttachmentDescription	attachmentDescription	=
	{
		vk::VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION,
		DE_NULL,
		mapToVkTextureFormat(format),					// format
		1u,												// samples
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,				// loadOp
		vk::VK_ATTACHMENT_STORE_OP_STORE,				// storeOp
		vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// stencilLoadOp
		vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			// stencilStoreOp
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// initialLayout
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// finalLayout
		0u,												// flags
	};
	const vk::VkAttachmentReference		colorAttachment			=
	{
		0u,												// attachment
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// layout
	};
	const vk::VkAttachmentReference		depthStencilAttachment	=
	{
		vk::VK_NO_ATTACHMENT,							// attachment
		vk::VK_IMAGE_LAYOUT_UNDEFINED					// layout
	};
	const vk::VkSubpassDescription		subpass					=
	{
		vk::VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION,
		DE_NULL,
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,			// pipelineBindPoint
		0u,												// flags
		0u,												// inputCount
		DE_NULL,										// inputAttachments
		1u,												// colorCount
		&colorAttachment,								// pColorAttachments
		DE_NULL,										// pResolveAttachments
		depthStencilAttachment,							// depthStencilAttachment
		0u,												// preserveCount
		DE_NULL											// pPreserveAttachments
	};
	const vk::VkRenderPassCreateInfo	renderPassCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		DE_NULL,
		1u,												// attachmentCount
		&attachmentDescription,							// pAttachments
		1u,												// subpassCount
		&subpass,										// pSubpasses
		0u,												// dependencyCount
		DE_NULL,										// pDependencies
	};

	return vk::createRenderPass(vki, device, &renderPassCreateInfo);
}

vk::Move<vk::VkFramebuffer> SingleTargetRenderInstance::createFramebuffer (const vk::DeviceInterface&	vki,
																		   vk::VkDevice					device,
																		   vk::VkRenderPass				renderpass,
																		   vk::VkImageView				colorAttachmentView,
																		   const tcu::UVec2&			size)
{
	const vk::VkFramebufferCreateInfo	framebufferCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		renderpass,				// renderPass
		1u,						// attachmentCount
		&colorAttachmentView,	// pAttachments
		size.x(),				// width
		size.y(),				// height
		1,						// layers
	};

	return vk::createFramebuffer(vki, device, &framebufferCreateInfo);
}

vk::Move<vk::VkCmdPool> SingleTargetRenderInstance::createCommandPool (const vk::DeviceInterface&	vki,
																	   vk::VkDevice					device,
																	   deUint32						queueFamilyIndex)
{
	const vk::VkCmdPoolCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,
		DE_NULL,
		queueFamilyIndex,						// queueFamilyIndex
		vk::VK_CMD_POOL_CREATE_TRANSIENT_BIT,	// flags
	};
	return vk::createCommandPool(vki, device, &createInfo);
}

void SingleTargetRenderInstance::readRenderTarget (tcu::TextureLevel& dst)
{
	const deUint64						pixelDataSize				= (deUint64)(m_targetSize.x() * m_targetSize.y() * m_targetFormat.getPixelSize());
	const vk::VkBufferCreateInfo		bufferCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		pixelDataSize,									// size
		vk::VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT,	// usage
		0u,												// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,					// sharingMode
		0u,												// queueFamilyCount
		DE_NULL,										// pQueueFamilyIndices
	};
	const vk::Unique<vk::VkBuffer>		buffer						(vk::createBuffer(m_vki, m_device, &bufferCreateInfo));
	const vk::VkImageSubresourceRange	fullSubrange				=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
		0u,												// baseMipLevel
		1u,												// mipLevels
		0u,												// baseArraySlice
		1u,												// arraySize
	};
	const vk::VkImageMemoryBarrier		imageBarrier				=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT,		// outputMask
		vk::VK_MEMORY_INPUT_TRANSFER_BIT,				// inputMask
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// oldLayout
		vk::VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,	// newLayout
		vk::VK_QUEUE_FAMILY_IGNORED,					// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,					// destQueueFamilyIndex
		*m_colorAttachmentImage,						// image
		fullSubrange,									// subresourceRange
	};
	const vk::VkBufferMemoryBarrier		memoryBarrier				=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_TRANSFER_BIT,				// outputMask
		vk::VK_MEMORY_INPUT_HOST_READ_BIT,				// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,					// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,					// destQueueFamilyIndex
		*buffer,										// buffer
		0u,												// offset
		(vk::VkDeviceSize)pixelDataSize					// size
	};
	const vk::VkCmdBufferCreateInfo		cmdBufCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
		DE_NULL,
		*m_cmdPool,							// cmdPool
		vk::VK_CMD_BUFFER_LEVEL_PRIMARY,	// level
		0u,									// flags
	};
	const vk::VkFenceCreateInfo			fenceCreateInfo				=
	{
		vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,
		0u,												// flags
	};
	const vk::VkCmdBufferBeginInfo		cmdBufBeginInfo				=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		vk::VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | vk::VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
		(vk::VkRenderPass)0u,																			// renderPass
		0u,																								// subpass
		(vk::VkFramebuffer)0u,																			// framebuffer
	};
	const vk::VkImageSubresourceCopy	firstSlice					=
	{
		vk::VK_IMAGE_ASPECT_COLOR,						// aspect
		0,												// mipLevel
		0,												// arrayLayer
		1,												// arraySize
	};
	const vk::VkBufferImageCopy			copyRegion					=
	{
		0u,												// bufferOffset
		m_targetSize.x(),								// bufferRowLength
		m_targetSize.y(),								// bufferImageHeight
		firstSlice,										// imageSubresource
		{ 0, 0, 0 },									// imageOffset
		{ (deInt32)m_targetSize.x(), (deInt32)m_targetSize.y(), 1 }		// imageExtent
	};

	const de::MovePtr<vk::Allocation>	bufferMemory				= allocateAndBindObjectMemory(m_vki, m_device, m_allocator, *buffer, vk::MemoryRequirement::HostVisible);

	const vk::Unique<vk::VkCmdBuffer>	cmd							(vk::createCommandBuffer(m_vki, m_device, &cmdBufCreateInfo));
	const vk::Unique<vk::VkFence>		cmdCompleteFence			(vk::createFence(m_vki, m_device, &fenceCreateInfo));
	const void* const					imageBarrierPtr				= &imageBarrier;
	const void* const					bufferBarrierPtr			= &memoryBarrier;
	const deUint64						infiniteTimeout				= ~(deUint64)0u;

	// copy content to buffer
	VK_CHECK(m_vki.beginCommandBuffer(*cmd, &cmdBufBeginInfo));
	m_vki.cmdPipelineBarrier(*cmd, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_FALSE, 1, &imageBarrierPtr);
	m_vki.cmdCopyImageToBuffer(*cmd, *m_colorAttachmentImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, *buffer, 1, &copyRegion);
	m_vki.cmdPipelineBarrier(*cmd, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_FALSE, 1, &bufferBarrierPtr);
	VK_CHECK(m_vki.endCommandBuffer(*cmd));

	// wait for transfer to complete
	VK_CHECK(m_vki.queueSubmit(m_queue, 1, &cmd.get(), *cmdCompleteFence));
	VK_CHECK(m_vki.waitForFences(m_device, 1, &cmdCompleteFence.get(), 0u, infiniteTimeout)); // \note: timeout is failure

	dst.setStorage(m_targetFormat, m_targetSize.x(), m_targetSize.y());

	// copy data
	invalidateMappedMemoryRange(m_vki, m_device, bufferMemory->getMemory(), bufferMemory->getOffset(), pixelDataSize);
	tcu::copy(dst, tcu::ConstPixelBufferAccess(dst.getFormat(), dst.getSize(), bufferMemory->getHostPtr()));
}

tcu::TestStatus SingleTargetRenderInstance::iterate (void)
{
	tcu::TextureLevel resultImage;

	// log
	if (m_firstIteration)
	{
		logTestPlan();
		m_firstIteration = false;
	}

	// render
	{
		// transition to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		const vk::VkImageSubresourceRange	fullSubrange				=
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
			0u,												// baseMipLevel
			1u,												// mipLevels
			0u,												// baseArraySlice
			1u,												// arraySize
		};
		const vk::VkImageMemoryBarrier		imageBarrier				=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,
			0u,												// outputMask
			vk::VK_MEMORY_INPUT_COLOR_ATTACHMENT_BIT,		// inputMask
			vk::VK_IMAGE_LAYOUT_UNDEFINED,					// oldLayout
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// newLayout
			vk::VK_QUEUE_FAMILY_IGNORED,					// srcQueueFamilyIndex
			vk::VK_QUEUE_FAMILY_IGNORED,					// destQueueFamilyIndex
			*m_colorAttachmentImage,						// image
			fullSubrange,									// subresourceRange
		};
		const vk::VkCmdBufferCreateInfo		cmdBufCreateInfo			=
		{
			vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
			DE_NULL,
			*m_cmdPool,										// cmdPool
			vk::VK_CMD_BUFFER_LEVEL_PRIMARY,				// level
			0u,												// flags
		};
		const vk::VkCmdBufferBeginInfo		cmdBufBeginInfo				=
		{
			vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
			DE_NULL,
			vk::VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | vk::VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
			(vk::VkRenderPass)0u,																			// renderPass
			0u,																								// subpass
			(vk::VkFramebuffer)0u,																			// framebuffer
		};

		const vk::Unique<vk::VkCmdBuffer>	cmd					(vk::createCommandBuffer(m_vki, m_device, &cmdBufCreateInfo));
		const void* const					imageBarrierPtr		= &imageBarrier;

		VK_CHECK(m_vki.beginCommandBuffer(*cmd, &cmdBufBeginInfo));
		m_vki.cmdPipelineBarrier(*cmd, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_FALSE, 1, &imageBarrierPtr);
		VK_CHECK(m_vki.endCommandBuffer(*cmd));
		VK_CHECK(m_vki.queueSubmit(m_queue, 1, &cmd.get(), 0));

		// and then render to
		renderToTarget();
	}

	// read and verify
	readRenderTarget(resultImage);
	return verifyResultImage(resultImage.getAccess());
}

class RenderInstanceShaders
{
public:
														RenderInstanceShaders		(const vk::DeviceInterface&		vki,
																					 vk::VkDevice					device,
																					 const vk::BinaryCollection&	programCollection);

	inline bool											hasTessellationStage		(void) const { return *m_tessCtrlShader != 0 || *m_tessEvalShader != 0;	}
	inline deUint32										getNumStages				(void) const { return (deUint32)m_stageInfos.size();					}
	inline const vk::VkPipelineShaderStageCreateInfo*	getStages					(void) const { return &m_stageInfos[0];									}

private:
	void												addStage					(const vk::DeviceInterface&		vki,
																					 vk::VkDevice					device,
																					 const vk::BinaryCollection&	programCollection,
																					 const char*					name,
																					 vk::VkShaderStage				stage,
																					 vk::Move<vk::VkShaderModule>*	outModule,
																					 vk::Move<vk::VkShader>*		outShader);

	vk::VkPipelineShaderStageCreateInfo					getShaderStageCreateInfo	(vk::VkShaderStage stage, vk::VkShader shader) const;

	vk::Move<vk::VkShaderModule>						m_vertexShaderModule;
	vk::Move<vk::VkShader>								m_vertexShader;
	vk::Move<vk::VkShaderModule>						m_tessCtrlShaderModule;
	vk::Move<vk::VkShader>								m_tessCtrlShader;
	vk::Move<vk::VkShaderModule>						m_tessEvalShaderModule;
	vk::Move<vk::VkShader>								m_tessEvalShader;
	vk::Move<vk::VkShaderModule>						m_geometryShaderModule;
	vk::Move<vk::VkShader>								m_geometryShader;
	vk::Move<vk::VkShaderModule>						m_fragmentShaderModule;
	vk::Move<vk::VkShader>								m_fragmentShader;
	std::vector<vk::VkPipelineShaderStageCreateInfo>	m_stageInfos;
};

RenderInstanceShaders::RenderInstanceShaders (const vk::DeviceInterface&	vki,
											  vk::VkDevice					device,
											  const vk::BinaryCollection&	programCollection)
{
	addStage(vki, device, programCollection, "vertex",		vk::VK_SHADER_STAGE_VERTEX,				&m_vertexShaderModule,		&m_vertexShader);
	addStage(vki, device, programCollection, "tess_ctrl",	vk::VK_SHADER_STAGE_TESS_CONTROL,		&m_tessCtrlShaderModule,	&m_tessCtrlShader);
	addStage(vki, device, programCollection, "tess_eval",	vk::VK_SHADER_STAGE_TESS_EVALUATION,	&m_tessEvalShaderModule,	&m_tessEvalShader);
	addStage(vki, device, programCollection, "geometry",	vk::VK_SHADER_STAGE_GEOMETRY,			&m_geometryShaderModule,	&m_geometryShader);
	addStage(vki, device, programCollection, "fragment",	vk::VK_SHADER_STAGE_FRAGMENT,			&m_fragmentShaderModule,	&m_fragmentShader);

	DE_ASSERT(!m_stageInfos.empty());
}

void RenderInstanceShaders::addStage (const vk::DeviceInterface&		vki,
									  vk::VkDevice						device,
									  const vk::BinaryCollection&		programCollection,
									  const char*						name,
									  vk::VkShaderStage					stage,
									  vk::Move<vk::VkShaderModule>*		outModule,
									  vk::Move<vk::VkShader>*			outShader)
{
	if (programCollection.contains(name))
	{
		vk::Move<vk::VkShaderModule>	module		= createShaderModule(vki, device, programCollection.get(name), (vk::VkShaderModuleCreateFlags)0);
		const vk::VkShaderCreateInfo	createInfo	=
		{
			vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,
			DE_NULL,
			*module,		// module
			"main",			// pName
			0u,				// flags
			stage
		};
		vk::Move<vk::VkShader>			shader		= vk::createShader(vki, device, &createInfo);

		m_stageInfos.push_back(getShaderStageCreateInfo(stage, *shader));
		*outModule = module;
		*outShader = shader;
	}
}

vk::VkPipelineShaderStageCreateInfo RenderInstanceShaders::getShaderStageCreateInfo (vk::VkShaderStage stage, vk::VkShader shader) const
{
	const vk::VkPipelineShaderStageCreateInfo	stageCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		stage,			// stage
		shader,			// shader
		DE_NULL,		// pSpecializationInfo
	};
	return stageCreateInfo;
}

class SingleCmdRenderInstance : public SingleTargetRenderInstance
{
public:
									SingleCmdRenderInstance	(Context&			context,
															 bool				isPrimaryCmdBuf,
															 const tcu::UVec2&	renderSize);

private:
	vk::Move<vk::VkPipeline>		createPipeline				(vk::VkPipelineLayout pipelineLayout);

	virtual vk::VkPipelineLayout	getPipelineLayout			(void) const = 0;
	virtual void					writeDrawCmdBuffer			(vk::VkCmdBuffer cmd) const = 0;

	void							renderToTarget				(void);

	const bool						m_isPrimaryCmdBuf;
};

SingleCmdRenderInstance::SingleCmdRenderInstance (Context&			context,
												  bool				isPrimaryCmdBuf,
												  const tcu::UVec2&	renderSize)
	: SingleTargetRenderInstance	(context, renderSize)
	, m_isPrimaryCmdBuf				(isPrimaryCmdBuf)
{
}

vk::Move<vk::VkPipeline> SingleCmdRenderInstance::createPipeline (vk::VkPipelineLayout pipelineLayout)
{
	const RenderInstanceShaders							shaderStages		(m_vki, m_device, m_context.getBinaryCollection());
	const vk::VkPrimitiveTopology						topology			= shaderStages.hasTessellationStage() ? vk::VK_PRIMITIVE_TOPOLOGY_PATCH : vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	const vk::VkPipelineVertexInputStateCreateInfo		vertexInputState	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		0u,											// bindingCount
		DE_NULL,									// pVertexBindingDescriptions
		0u,											// attributeCount
		DE_NULL,									// pVertexAttributeDescriptions
	};
	const vk::VkPipelineInputAssemblyStateCreateInfo	iaState				=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		DE_NULL,
		topology,									// topology
		vk::VK_FALSE,								// primitiveRestartEnable
	};
	const vk::VkPipelineTessellationStateCreateInfo		tessState			=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		DE_NULL,
		3u,											// patchControlPoints
	};
	const vk::VkViewport								viewport			=
	{
		0.0f,										// originX
		0.0f,										// originY
		float(m_targetSize.x()),					// width
		float(m_targetSize.y()),					// height
		0.0f,										// minDepth
		1.0f,										// maxDepth
	};
	const vk::VkRect2D									renderArea			=
	{
		{ 0, 0 },													// offset
		{ (deInt32)m_targetSize.x(), (deInt32)m_targetSize.y() },	// extent
	};
	const vk::VkPipelineViewportStateCreateInfo			vpState				=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		DE_NULL,
		1u,											// viewportCount
		&viewport,
		1u,
		&renderArea,
	};
	const vk::VkPipelineRasterStateCreateInfo			rsState				=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO,
		DE_NULL,
		vk::VK_TRUE,								// depthClipEnable
		vk::VK_FALSE,								// rasterizerDiscardEnable
		vk::VK_FILL_MODE_SOLID,						// fillMode
		vk::VK_CULL_MODE_NONE,						// cullMode
		vk::VK_FRONT_FACE_CCW,						// frontFace
		vk::VK_FALSE,								// depthBiasEnable
		0.0f,										// depthBias
		0.0f,										// depthBiasClamp
		0.0f,										// slopeScaledDepthBias
		1.0f,										// lineWidth
	};
	const vk::VkSampleMask								sampleMask			= 0x01u;
	const vk::VkPipelineMultisampleStateCreateInfo		msState				=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		DE_NULL,
		1u,											// rasterSamples
		vk::VK_FALSE,								// sampleShadingEnable
		0.0f,										// minSampleShading
		&sampleMask									// sampleMask
	};
	const vk::VkPipelineDepthStencilStateCreateInfo		dsState				=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		DE_NULL,
		vk::VK_FALSE,								// depthTestEnable
		vk::VK_FALSE,								// depthWriteEnable
		vk::VK_COMPARE_OP_ALWAYS,					// depthCompareOp
		vk::VK_FALSE,								// depthBoundsTestEnable
		vk::VK_FALSE,								// stencilTestEnable
		{ vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_ALWAYS, 0u, 0u, 0u },	// front
		{ vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_ALWAYS, 0u, 0u, 0u },	// back
		-1.0f,										// minDepthBounds
		+1.0f,										// maxDepthBounds
	};
	const vk::VkPipelineColorBlendAttachmentState		cbAttachment		=
	{
		vk::VK_FALSE,								// blendEnable
		vk::VK_BLEND_ZERO,							// srcBlendColor
		vk::VK_BLEND_ZERO,							// destBlendColor
		vk::VK_BLEND_OP_ADD,						// blendOpColor
		vk::VK_BLEND_ZERO,							// srcBlendAlpha
		vk::VK_BLEND_ZERO,							// destBlendAlpha
		vk::VK_BLEND_OP_ADD,						// blendOpAlpha
		vk::VK_CHANNEL_R_BIT | vk::VK_CHANNEL_G_BIT | vk::VK_CHANNEL_B_BIT | vk::VK_CHANNEL_A_BIT,	// channelWriteMask
	};
	const vk::VkPipelineColorBlendStateCreateInfo		cbState				=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		DE_NULL,
		vk::VK_FALSE,								// alphaToCoverageEnable
		vk::VK_FALSE,								// alphaToOneEnable
		vk::VK_FALSE,								// logicOpEnable
		vk::VK_LOGIC_OP_CLEAR,						// logicOp
		1u,											// attachmentCount
		&cbAttachment,								// pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f },					// blendConst
	};
	const vk::VkPipelineDynamicStateCreateInfo			dynState			=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		DE_NULL,
		0u,											// dynamicStateCount
		DE_NULL,									// pDynamicStates
	};
	const vk::VkGraphicsPipelineCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		DE_NULL,
		shaderStages.getNumStages(),									// stageCount
		shaderStages.getStages(),										// pStages
		&vertexInputState,												// pVertexInputState
		&iaState,														// pInputAssemblyState
		(shaderStages.hasTessellationStage() ? &tessState : DE_NULL),	// pTessellationState
		&vpState,														// pViewportState
		&rsState,														// pRasterState
		&msState,														// pMultisampleState
		&dsState,														// pDepthStencilState
		&cbState,														// pColorBlendState
		&dynState,														// pDynamicState
		0u,																// flags
		pipelineLayout,													// layout
		*m_renderPass,													// renderPass
		0u,																// subpass
		(vk::VkPipeline)0,												// basePipelineHandle
		0u,																// basePipelineIndex
	};
	return createGraphicsPipeline(m_vki, m_device, (vk::VkPipelineCache)0u, &createInfo);
}

void SingleCmdRenderInstance::renderToTarget (void)
{
	const vk::VkRect2D									renderArea					=
	{
		{ 0, 0 },													// offset
		{ (deInt32)m_targetSize.x(), (deInt32)m_targetSize.y() },	// extent
	};
	const vk::VkCmdBufferCreateInfo						mainCmdBufCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
		DE_NULL,
		*m_cmdPool,							// cmdPool
		vk::VK_CMD_BUFFER_LEVEL_PRIMARY,	// level
		0u,									// flags
	};
	const vk::VkCmdBufferBeginInfo						mainCmdBufBeginInfo				=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		vk::VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | vk::VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
		(vk::VkRenderPass)0u,																			// renderPass
		0u,																								// subpass
		(vk::VkFramebuffer)0u,																			// framebuffer
	};
	const vk::VkCmdBufferCreateInfo						passCmdBufCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
		DE_NULL,
		*m_cmdPool,							// cmdPool
		vk::VK_CMD_BUFFER_LEVEL_SECONDARY,	// level
		0u,									// flags
	};
	const vk::VkCmdBufferBeginInfo						passCmdBufBeginInfo				=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		vk::VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | vk::VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
		(vk::VkRenderPass)*m_renderPass,																// renderPass
		0u,																								// subpass
		(vk::VkFramebuffer)*m_framebuffer,																// framebuffer
	};
	const vk::VkFenceCreateInfo							fenceCreateInfo				=
	{
		vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,
		0u,			// flags
	};
	const vk::VkClearValue								clearValue					= createClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
	const vk::VkRenderPassBeginInfo						renderPassBeginInfo			=
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		DE_NULL,
		*m_renderPass,		// renderPass
		*m_framebuffer,		// framebuffer
		renderArea,			// renderArea
		1u,					// clearValueCount
		&clearValue,		// pClearValues
	};

	const vk::VkPipelineLayout							pipelineLayout				(getPipelineLayout());
	const vk::Unique<vk::VkPipeline>					pipeline					(createPipeline(pipelineLayout));
	const vk::Unique<vk::VkCmdBuffer>					mainCmd						(vk::createCommandBuffer(m_vki, m_device, &mainCmdBufCreateInfo));
	const vk::Unique<vk::VkCmdBuffer>					passCmd						((m_isPrimaryCmdBuf) ? (vk::Move<vk::VkCmdBuffer>()) : (vk::createCommandBuffer(m_vki, m_device, &passCmdBufCreateInfo)));
	const vk::Unique<vk::VkFence>						fence						(vk::createFence(m_vki, m_device, &fenceCreateInfo));
	const deUint64										infiniteTimeout				= ~(deUint64)0u;
	const vk::VkRenderPassContents						passContents				= (m_isPrimaryCmdBuf) ? (vk::VK_RENDER_PASS_CONTENTS_INLINE) : (vk::VK_RENDER_PASS_CONTENTS_SECONDARY_CMD_BUFFERS);

	VK_CHECK(m_vki.beginCommandBuffer(*mainCmd, &mainCmdBufBeginInfo));
	m_vki.cmdBeginRenderPass(*mainCmd, &renderPassBeginInfo, passContents);

	if (m_isPrimaryCmdBuf)
	{
		m_vki.cmdBindPipeline(*mainCmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		writeDrawCmdBuffer(*mainCmd);
	}
	else
	{
		VK_CHECK(m_vki.beginCommandBuffer(*passCmd, &passCmdBufBeginInfo));
		m_vki.cmdBindPipeline(*passCmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		writeDrawCmdBuffer(*passCmd);
		VK_CHECK(m_vki.endCommandBuffer(*passCmd));

		m_vki.cmdExecuteCommands(*mainCmd, 1, &passCmd.get());
	}

	m_vki.cmdEndRenderPass(*mainCmd);
	VK_CHECK(m_vki.endCommandBuffer(*mainCmd));

	// submit and wait for them to finish before exiting scope. (Killing in-flight objects is a no-no).
	VK_CHECK(m_vki.queueSubmit(m_queue, 1, &mainCmd.get(), *fence));
	VK_CHECK(m_vki.waitForFences(m_device, 1, &fence.get(), 0u, infiniteTimeout)); // \note: timeout is failure
}

enum ShaderInputInterface
{
	SHADER_INPUT_SINGLE_DESCRIPTOR = 0,	//!< one descriptor
	SHADER_INPUT_MULTIPLE_DESCRIPTORS,	//!< multiple descriptors
	SHADER_INPUT_DESCRIPTOR_ARRAY,		//!< descriptor array

	SHADER_INPUT_LAST
};

deUint32 getInterfaceNumResources (ShaderInputInterface shaderInterface)
{
	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:	return 1u;
		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:	return 2u;
		case SHADER_INPUT_DESCRIPTOR_ARRAY:		return 2u;

		default:
			DE_FATAL("Impossible");
			return 0u;
	}
}

class BufferRenderInstance : public SingleCmdRenderInstance
{
public:
													BufferRenderInstance		(Context&					context,
																				 bool						isPrimaryCmdBuf,
																				 vk::VkDescriptorType		descriptorType,
																				 vk::VkShaderStageFlags		stageFlags,
																				 ShaderInputInterface		shaderInterface,
																				 bool						viewOffset,
																				 bool						dynamicOffset,
																				 bool						dynamicOffsetNonZero);

	static vk::Move<vk::VkBuffer>					createSourceBuffer			(const vk::DeviceInterface&		vki,
																				 vk::VkDevice					device,
																				 vk::Allocator&					allocator,
																				 vk::VkDescriptorType			descriptorType,
																				 deUint32						offset,
																				 deUint32						bufferSize,
																				 de::MovePtr<vk::Allocation>*	outMemory);

	static vk::Move<vk::VkDescriptorPool>			createDescriptorPool		(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface);

	static vk::Move<vk::VkDescriptorSetLayout>		createDescriptorSetLayout	(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface,
																				 vk::VkShaderStageFlags		stageFlags);

	static vk::Move<vk::VkDescriptorSet>			createDescriptorSet			(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorSetLayout	descriptorSetLayout,
																				 vk::VkDescriptorPool		descriptorPool,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface,
																				 vk::VkBuffer				sourceBufferA,
																				 const deUint32				viewOffsetA,
																				 vk::VkBuffer				sourceBufferB,
																				 const deUint32				viewOffsetB);

	static vk::Move<vk::VkPipelineLayout>			createPipelineLayout		(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorSetLayout	descriptorSetLayout);

	void											logTestPlan					(void) const;
	vk::VkPipelineLayout							getPipelineLayout			(void) const;
	void											writeDrawCmdBuffer			(vk::VkCmdBuffer cmd) const;
	tcu::TestStatus									verifyResultImage			(const tcu::ConstPixelBufferAccess& result) const;

	enum
	{
		RENDER_SIZE				= 128,
		BUFFER_DATA_SIZE		= 8 * sizeof(float),
		BUFFER_SIZE_A			= 2048, //!< a lot more than required
		BUFFER_SIZE_B			= 2560, //!< a lot more than required

		STATIC_OFFSET_VALUE_A	= 256,
		DYNAMIC_OFFSET_VALUE_A	= 512,
		STATIC_OFFSET_VALUE_B	= 1024,
		DYNAMIC_OFFSET_VALUE_B	= 768,
	};

	const vk::VkDescriptorType						m_descriptorType;
	const ShaderInputInterface						m_shaderInterface;
	const bool										m_setViewOffset;
	const bool										m_setDynamicOffset;
	const bool										m_dynamicOffsetNonZero;
	const vk::VkShaderStageFlags					m_stageFlags;

	const deUint32									m_viewOffsetA;
	const deUint32									m_viewOffsetB;
	const deUint32									m_dynamicOffsetA;
	const deUint32									m_dynamicOffsetB;
	const deUint32									m_effectiveOffsetA;
	const deUint32									m_effectiveOffsetB;
	const deUint32									m_bufferSizeA;
	const deUint32									m_bufferSizeB;

	de::MovePtr<vk::Allocation>						m_bufferMemoryA;
	de::MovePtr<vk::Allocation>						m_bufferMemoryB;
	const vk::Unique<vk::VkBuffer>					m_sourceBufferA;
	const vk::Unique<vk::VkBuffer>					m_sourceBufferB;
	const vk::Unique<vk::VkDescriptorPool>			m_descriptorPool;
	const vk::Unique<vk::VkDescriptorSetLayout>		m_descriptorSetLayout;
	const vk::Unique<vk::VkDescriptorSet>			m_descriptorSet;
	const vk::Unique<vk::VkPipelineLayout>			m_pipelineLayout;
};

BufferRenderInstance::BufferRenderInstance	(Context&				context,
											 bool					isPrimaryCmdBuf,
											 vk::VkDescriptorType	descriptorType,
											 vk::VkShaderStageFlags	stageFlags,
											 ShaderInputInterface	shaderInterface,
											 bool					viewOffset,
											 bool					dynamicOffset,
											 bool					dynamicOffsetNonZero)
	: SingleCmdRenderInstance		(context, isPrimaryCmdBuf, tcu::UVec2(RENDER_SIZE, RENDER_SIZE))
	, m_descriptorType				(descriptorType)
	, m_shaderInterface				(shaderInterface)
	, m_setViewOffset				(viewOffset)
	, m_setDynamicOffset			(dynamicOffset)
	, m_dynamicOffsetNonZero		(dynamicOffsetNonZero)
	, m_stageFlags					(stageFlags)
	, m_viewOffsetA					((m_setViewOffset) ? ((deUint32)STATIC_OFFSET_VALUE_A) : (0u))
	, m_viewOffsetB					((m_setViewOffset) ? ((deUint32)STATIC_OFFSET_VALUE_B) : (0u))
	, m_dynamicOffsetA				((dynamicOffsetNonZero) ? ((deUint32)DYNAMIC_OFFSET_VALUE_A) : (0u))
	, m_dynamicOffsetB				((dynamicOffsetNonZero) ? ((deUint32)DYNAMIC_OFFSET_VALUE_B) : (0u))
	, m_effectiveOffsetA			((isDynamicDescriptorType(m_descriptorType)) ? (m_viewOffsetA + m_dynamicOffsetA) : (m_viewOffsetA))
	, m_effectiveOffsetB			((isDynamicDescriptorType(m_descriptorType)) ? (m_viewOffsetB + m_dynamicOffsetB) : (m_viewOffsetB))
	, m_bufferSizeA					(BUFFER_SIZE_A)
	, m_bufferSizeB					(BUFFER_SIZE_B)
	, m_bufferMemoryA				(DE_NULL)
	, m_bufferMemoryB				(DE_NULL)
	, m_sourceBufferA				(createSourceBuffer(m_vki, m_device, m_allocator, m_descriptorType, m_effectiveOffsetA, m_bufferSizeA, &m_bufferMemoryA))
	, m_sourceBufferB				((getInterfaceNumResources(m_shaderInterface) == 1u)
										? vk::Move<vk::VkBuffer>()
										: createSourceBuffer(m_vki, m_device, m_allocator, m_descriptorType, m_effectiveOffsetB, m_bufferSizeB, &m_bufferMemoryB))
	, m_descriptorPool				(createDescriptorPool(m_vki, m_device, m_descriptorType, m_shaderInterface))
	, m_descriptorSetLayout			(createDescriptorSetLayout(m_vki, m_device, m_descriptorType, m_shaderInterface, m_stageFlags))
	, m_descriptorSet				(createDescriptorSet(m_vki, m_device, *m_descriptorSetLayout, *m_descriptorPool, m_descriptorType, m_shaderInterface, *m_sourceBufferA, m_viewOffsetA, *m_sourceBufferB, m_viewOffsetB))
	, m_pipelineLayout				(createPipelineLayout(m_vki, m_device, *m_descriptorSetLayout))
{
	if (m_setDynamicOffset)
		DE_ASSERT(isDynamicDescriptorType(m_descriptorType));
	if (m_dynamicOffsetNonZero)
		DE_ASSERT(m_setDynamicOffset);
}

vk::Move<vk::VkBuffer> BufferRenderInstance::createSourceBuffer (const vk::DeviceInterface&		vki,
																 vk::VkDevice					device,
																 vk::Allocator&					allocator,
																 vk::VkDescriptorType			descriptorType,
																 deUint32						offset,
																 deUint32						bufferSize,
																 de::MovePtr<vk::Allocation>*	outMemory)
{
	static const float				s_colors[]			=
	{
		0.0f, 1.0f, 0.0f, 1.0f,		// green
		1.0f, 1.0f, 0.0f, 1.0f,		// yellow
	};
	DE_STATIC_ASSERT(sizeof(s_colors) == BUFFER_DATA_SIZE);
	DE_ASSERT(offset + BUFFER_DATA_SIZE <= bufferSize);
	DE_ASSERT(offset % sizeof(float) == 0);
	DE_ASSERT(bufferSize % sizeof(float) == 0);

	const bool						isUniformBuffer		= isUniformDescriptorType(descriptorType);
	const vk::VkBufferUsageFlags	usageFlags			= (isUniformBuffer) ? (vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) : (vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	const float						preGuardValue		= 0.5f;
	const float						postGuardValue		= 0.75f;
	const vk::VkBufferCreateInfo	bufferCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		bufferSize,						// size
		usageFlags,						// usage
		0u,								// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,	// sharingMode
		0u,								// queueFamilyCount
		DE_NULL,						// pQueueFamilyIndices
	};
	vk::Move<vk::VkBuffer>			buffer				(vk::createBuffer(vki, device, &bufferCreateInfo));
	de::MovePtr<vk::Allocation>		bufferMemory		= allocateAndBindObjectMemory(vki, device, allocator, *buffer, vk::MemoryRequirement::HostVisible);
	void* const						mapPtr				= bufferMemory->getHostPtr();

	// guard with interesting values
	for (size_t preGuardOffset = 0; preGuardOffset + sizeof(float) <= (size_t)offset; preGuardOffset += sizeof(float))
		deMemcpy((deUint8*)mapPtr + preGuardOffset, &preGuardValue, sizeof(float));

	deMemcpy((deUint8*)mapPtr + offset, s_colors, sizeof(s_colors));
	for (size_t postGuardOffset = (size_t)offset + sizeof(s_colors); postGuardOffset + sizeof(float) <= (size_t)bufferSize; postGuardOffset += sizeof(float))
		deMemcpy((deUint8*)mapPtr + postGuardOffset, &postGuardValue, sizeof(float));
	deMemset((deUint8*)mapPtr + offset + sizeof(s_colors), 0x5A, (size_t)bufferSize - (size_t)offset - sizeof(s_colors)); // fill with interesting pattern that produces valid floats

	flushMappedMemoryRange(vki, device, bufferMemory->getMemory(), bufferMemory->getOffset(), bufferSize);

	*outMemory = bufferMemory;
	return buffer;
}

vk::Move<vk::VkDescriptorPool> BufferRenderInstance::createDescriptorPool (const vk::DeviceInterface&	vki,
																		   vk::VkDevice					device,
																		   vk::VkDescriptorType			descriptorType,
																		   ShaderInputInterface			shaderInterface)
{
	return vk::DescriptorPoolBuilder()
		.addType(descriptorType, getInterfaceNumResources(shaderInterface))
		.build(vki, device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);
}

vk::Move<vk::VkDescriptorSetLayout> BufferRenderInstance::createDescriptorSetLayout (const vk::DeviceInterface&	vki,
																					 vk::VkDevice				device,
																					 vk::VkDescriptorType		descriptorType,
																					 ShaderInputInterface		shaderInterface,
																					 vk::VkShaderStageFlags		stageFlags)
{
	vk::DescriptorSetLayoutBuilder builder;

	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.addSingleBinding(descriptorType, stageFlags);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.addSingleBinding(descriptorType, stageFlags);
			builder.addSingleBinding(descriptorType, stageFlags);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.addArrayBinding(descriptorType, 2u, stageFlags);
			break;

		default:
			DE_FATAL("Impossible");
	}

	return builder.build(vki, device);
}

vk::Move<vk::VkDescriptorSet> BufferRenderInstance::createDescriptorSet (const vk::DeviceInterface&	vki,
																		 vk::VkDevice				device,
																		 vk::VkDescriptorSetLayout	descriptorSetLayout,
																		 vk::VkDescriptorPool		descriptorPool,
																		 vk::VkDescriptorType		descriptorType,
																		 ShaderInputInterface		shaderInterface,
																		 vk::VkBuffer				bufferA,
																		 deUint32					offsetA,
																		 vk::VkBuffer				bufferB,
																		 deUint32					offsetB)
{
	const vk::VkDescriptorInfo		bufferInfos[2]	=
	{
		createDescriptorInfo(bufferA, (vk::VkDeviceSize)offsetA, (vk::VkDeviceSize)BUFFER_DATA_SIZE),
		createDescriptorInfo(bufferB, (vk::VkDeviceSize)offsetB, (vk::VkDeviceSize)BUFFER_DATA_SIZE),
	};

	vk::Move<vk::VkDescriptorSet>	descriptorSet	= allocDescriptorSet(vki, device, descriptorPool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, descriptorSetLayout);
	vk::DescriptorSetUpdateBuilder	builder;

	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &bufferInfos[0]);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &bufferInfos[0]);
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), descriptorType, &bufferInfos[1]);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.writeArray(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, 2u, bufferInfos);
			break;

		default:
			DE_FATAL("Impossible");
	}

	builder.update(vki, device);
	return descriptorSet;
}

vk::Move<vk::VkPipelineLayout> BufferRenderInstance::createPipelineLayout (const vk::DeviceInterface&	vki,
																		   vk::VkDevice					device,
																		   vk::VkDescriptorSetLayout	descriptorSetLayout)
{
	const vk::VkPipelineLayoutCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		1,						// descriptorSetCount
		&descriptorSetLayout,	// pSetLayouts
		0u,						// pushConstantRangeCount
		DE_NULL,				// pPushConstantRanges
	};

	return vk::createPipelineLayout(vki, device, &createInfo);
}

void BufferRenderInstance::logTestPlan (void) const
{
	std::ostringstream msg;

	msg << "Rendering 2x2 yellow-green grid.\n"
		<< "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
		<< " descriptor(s) of type " << vk::getDescriptorTypeName(m_descriptorType) << "\n"
		<< "Buffer view(s) have " << ((m_setViewOffset) ? ("non-") : ("")) << "zero offset.\n";

	if (isDynamicDescriptorType(m_descriptorType))
	{
		if (m_setDynamicOffset)
		{
			msg << "Source buffer(s) are given a dynamic offset at bind time.\n"
				<< "The supplied dynamic offset is " << ((m_dynamicOffsetNonZero) ? ("non-") : ("")) << "zero.\n";
		}
		else
		{
			msg << "Dynamic offset is not supplied at bind time. Expecting bind to offset 0.\n";
		}
	}

	if (m_stageFlags == 0u)
	{
		msg << "Descriptors are not accessed in any shader stage.\n";
	}
	else
	{
		msg << "Descriptors are accessed in {"
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_VERTEX_BIT) != 0)			? (" vertex")			: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_TESS_CONTROL_BIT) != 0)	? (" tess_control")		: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT) != 0)	? (" tess_evaluation")	: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_GEOMETRY_BIT) != 0)		? (" geometry")			: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_FRAGMENT_BIT) != 0)		? (" fragment")			: (""))
			<< " } stages.\n";
	}

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< msg.str()
		<< tcu::TestLog::EndMessage;
}

vk::VkPipelineLayout BufferRenderInstance::getPipelineLayout (void) const
{
	return *m_pipelineLayout;
}

void BufferRenderInstance::writeDrawCmdBuffer (vk::VkCmdBuffer cmd) const
{
	const bool							isUniformBuffer		= isUniformDescriptorType(m_descriptorType);

	// \note dynamic offset replaces the view offset, i.e. it is not offset relative to the view offset
	const deUint32						dynamicOffsets[]	=
	{
		m_dynamicOffsetA,
		m_dynamicOffsetB,
	};
	const deUint32						numOffsets			= (!m_setDynamicOffset) ? (0u) : (getInterfaceNumResources(m_shaderInterface));
	const deUint32* const				dynamicOffsetPtr	= (!m_setDynamicOffset) ? (DE_NULL) : (dynamicOffsets);

	// make host writes device-visible
	const vk::VkMemoryInputFlags		inputBit			= (isUniformBuffer) ? (vk::VK_MEMORY_INPUT_UNIFORM_READ_BIT) : (vk::VK_MEMORY_INPUT_SHADER_READ_BIT);
	const vk::VkBufferMemoryBarrier		memoryBarrierA		=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_HOST_WRITE_BIT,		// outputMask
		inputBit,									// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
		*m_sourceBufferA,							// buffer
		0u,											// offset
		(vk::VkDeviceSize)m_bufferSizeA,			// size
	};
	const vk::VkBufferMemoryBarrier		memoryBarrierB		=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_HOST_WRITE_BIT,		// outputMask
		inputBit,									// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
		*m_sourceBufferB,							// buffer
		0u,											// offset
		(vk::VkDeviceSize)m_bufferSizeB,			// size
	};
	const void* const					memoryBarriers[2]	=
	{
		&memoryBarrierA,
		&memoryBarrierB,
	};
	const deUint32						numMemoryBarriers	= getInterfaceNumResources(m_shaderInterface);

	m_vki.cmdBindDescriptorSets(cmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, getPipelineLayout(), 0, 1, &m_descriptorSet.get(), numOffsets, dynamicOffsetPtr);
	m_vki.cmdPipelineBarrier(cmd, 0u, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS, vk::VK_FALSE, numMemoryBarriers, memoryBarriers);
	m_vki.cmdDraw(cmd, 6 * 4, 1, 0, 0); // render four quads (two separate triangles)
}

tcu::TestStatus BufferRenderInstance::verifyResultImage (const tcu::ConstPixelBufferAccess& result) const
{
	const tcu::Vec4		green		(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4		yellow		(1.0f, 1.0f, 0.0f, 1.0f);
	tcu::Surface		reference	(m_targetSize.x(), m_targetSize.y());

	drawQuadrantReferenceResult(reference.getAccess(), yellow, green, green, yellow);

	if (!bilinearCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", reference.getAccess(), result, tcu::RGBA(1, 1, 1, 1), tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Image verification failed");
	else
		return tcu::TestStatus::pass("Pass");
}

class ComputeInstanceResultBuffer
{
public:
	enum
	{
		DATA_SIZE = sizeof(tcu::Vec4[4])
	};

											ComputeInstanceResultBuffer	(const vk::DeviceInterface&		vki,
																		 vk::VkDevice					device,
																		 vk::Allocator&					allocator);

	void									readResultContentsTo		(tcu::Vec4 (*results)[4]) const;

	inline vk::VkBuffer						getBuffer					(void) const { return *m_buffer;			}
	inline const void*						getResultReadBarrier		(void) const { return &m_bufferBarrier;		}

private:
	static vk::Move<vk::VkBuffer>			createResultBuffer			(const vk::DeviceInterface&		vki,
																		 vk::VkDevice					device,
																		 vk::Allocator&					allocator,
																		 de::MovePtr<vk::Allocation>*	outAllocation);

	static vk::VkBufferMemoryBarrier		createResultBufferBarrier	(vk::VkBuffer buffer);

	const vk::DeviceInterface&				m_vki;
	const vk::VkDevice						m_device;

	de::MovePtr<vk::Allocation>				m_bufferMem;
	const vk::Unique<vk::VkBuffer>			m_buffer;
	const vk::VkBufferMemoryBarrier			m_bufferBarrier;
};

ComputeInstanceResultBuffer::ComputeInstanceResultBuffer (const vk::DeviceInterface&	vki,
														  vk::VkDevice					device,
														  vk::Allocator&				allocator)
	: m_vki				(vki)
	, m_device			(device)
	, m_bufferMem		(DE_NULL)
	, m_buffer			(createResultBuffer(m_vki, m_device, allocator, &m_bufferMem))
	, m_bufferBarrier	(createResultBufferBarrier(*m_buffer))
{
}

void ComputeInstanceResultBuffer::readResultContentsTo (tcu::Vec4 (*results)[4]) const
{
	invalidateMappedMemoryRange(m_vki, m_device, m_bufferMem->getMemory(), m_bufferMem->getOffset(), sizeof(*results));
	deMemcpy(*results, m_bufferMem->getHostPtr(), sizeof(*results));
}

vk::Move<vk::VkBuffer> ComputeInstanceResultBuffer::createResultBuffer (const vk::DeviceInterface&		vki,
																		vk::VkDevice					device,
																		vk::Allocator&					allocator,
																		de::MovePtr<vk::Allocation>*	outAllocation)
{
	const vk::VkBufferCreateInfo	createInfo	=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		(vk::VkDeviceSize)DATA_SIZE,				// size
		vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// usage
		0u,											// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		0u,											// queueFamilyCount
		DE_NULL,									// pQueueFamilyIndices
	};
	vk::Move<vk::VkBuffer>			buffer		(vk::createBuffer(vki, device, &createInfo));
	de::MovePtr<vk::Allocation>		allocation	(allocateAndBindObjectMemory(vki, device, allocator, *buffer, vk::MemoryRequirement::HostVisible));
	const float						clearValue	= -1.0f;
	void*							mapPtr		= allocation->getHostPtr();

	for (size_t offset = 0; offset < DATA_SIZE; offset += sizeof(float))
		deMemcpy(((deUint8*)mapPtr) + offset, &clearValue, sizeof(float));

	flushMappedMemoryRange(vki, device, allocation->getMemory(), allocation->getOffset(), (vk::VkDeviceSize)DATA_SIZE);

	*outAllocation = allocation;
	return buffer;
}

vk::VkBufferMemoryBarrier ComputeInstanceResultBuffer::createResultBufferBarrier (vk::VkBuffer buffer)
{
	const vk::VkBufferMemoryBarrier bufferBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_SHADER_WRITE_BIT,		// outputMask
		vk::VK_MEMORY_INPUT_HOST_READ_BIT,			// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
		buffer,										// buffer
		(vk::VkDeviceSize)0u,						// offset
		DATA_SIZE,									// size
	};
	return bufferBarrier;
}

class ComputePipeline
{
public:
											ComputePipeline			(const vk::DeviceInterface&			vki,
																	 vk::VkDevice						device,
																	 const vk::BinaryCollection&		programCollection,
																	 deUint32							numDescriptorSets,
																	 const vk::VkDescriptorSetLayout*	descriptorSetLayouts);

	inline vk::VkPipeline					getPipeline				(void) const { return *m_pipeline;			};
	inline vk::VkPipelineLayout				getPipelineLayout		(void) const { return *m_pipelineLayout;	};

private:
	static vk::Move<vk::VkPipelineLayout>	createPipelineLayout	(const vk::DeviceInterface&			vki,
																	 vk::VkDevice						device,
																	 deUint32							numDescriptorSets,
																	 const vk::VkDescriptorSetLayout*	descriptorSetLayouts);

	static vk::Move<vk::VkPipeline>			createPipeline			(const vk::DeviceInterface&			vki,
																	 vk::VkDevice						device,
																	 const vk::BinaryCollection&		programCollection,
																	 vk::VkPipelineLayout				layout);

	const vk::Unique<vk::VkPipelineLayout>	m_pipelineLayout;
	const vk::Unique<vk::VkPipeline>		m_pipeline;
};

ComputePipeline::ComputePipeline (const vk::DeviceInterface&		vki,
								  vk::VkDevice						device,
								  const vk::BinaryCollection&		programCollection,
								  deUint32							numDescriptorSets,
								  const vk::VkDescriptorSetLayout*	descriptorSetLayouts)
	: m_pipelineLayout	(createPipelineLayout(vki, device, numDescriptorSets, descriptorSetLayouts))
	, m_pipeline		(createPipeline(vki, device, programCollection, *m_pipelineLayout))
{
}

vk::Move<vk::VkPipelineLayout> ComputePipeline::createPipelineLayout (const vk::DeviceInterface&		vki,
																	  vk::VkDevice						device,
																	  deUint32							numDescriptorSets,
																	  const vk::VkDescriptorSetLayout*	descriptorSetLayouts)
{
	const vk::VkPipelineLayoutCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		numDescriptorSets,		// descriptorSetCount
		descriptorSetLayouts,	// pSetLayouts
		0u,						// pushConstantRangeCount
		DE_NULL,				// pPushConstantRanges
	};
	return vk::createPipelineLayout(vki, device, &createInfo);
}

vk::Move<vk::VkPipeline> ComputePipeline::createPipeline (const vk::DeviceInterface&	vki,
														  vk::VkDevice					device,
														  const vk::BinaryCollection&	programCollection,
														  vk::VkPipelineLayout			layout)
{
	const vk::Unique<vk::VkShaderModule>		computeModule		(vk::createShaderModule(vki, device, programCollection.get("compute"), (vk::VkShaderModuleCreateFlags)0u));
	const vk::VkShaderCreateInfo				shaderCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,
		DE_NULL,
		*computeModule,		// module
		"main",				// pName
		0u,					// flags
		vk::VK_SHADER_STAGE_COMPUTE
	};
	const vk::Unique<vk::VkShader>				computeShader		(vk::createShader(vki, device, &shaderCreateInfo));
	const vk::VkPipelineShaderStageCreateInfo	cs					=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		vk::VK_SHADER_STAGE_COMPUTE,	// stage
		*computeShader,					// shader
		DE_NULL,						// pSpecializationInfo
	};
	const vk::VkComputePipelineCreateInfo		createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		cs,								// cs
		0u,								// flags
		layout,							// layout
		(vk::VkPipeline)0,				// basePipelineHandle
		0u,								// basePipelineIndex
	};
	return createComputePipeline(vki, device, (vk::VkPipelineCache)0u, &createInfo);
}

class ComputeCommand
{
public:
										ComputeCommand	(const vk::DeviceInterface&	vki,
														 vk::VkDevice				device,
														 vk::VkPipeline				pipeline,
														 vk::VkPipelineLayout		pipelineLayout,
														 const tcu::UVec3&			numWorkGroups,
														 int						numDescriptorSets,
														 const vk::VkDescriptorSet*	descriptorSets,
														 int						numDynamicOffsets,
														 const deUint32*			dynamicOffsets,
														 int						numPreBarriers,
														 const void* const*			preBarriers,
														 int						numPostBarriers,
														 const void* const*			postBarriers);

	void								submitAndWait	(deUint32 queueFamilyIndex, vk::VkQueue queue) const;

private:
	const vk::DeviceInterface&			m_vki;
	const vk::VkDevice					m_device;
	const vk::VkPipeline				m_pipeline;
	const vk::VkPipelineLayout			m_pipelineLayout;
	const tcu::UVec3&					m_numWorkGroups;
	const int							m_numDescriptorSets;
	const vk::VkDescriptorSet* const	m_descriptorSets;
	const int							m_numDynamicOffsets;
	const deUint32* const				m_dynamicOffsets;
	const int							m_numPreBarriers;
	const void* const* const			m_preBarriers;
	const int							m_numPostBarriers;
	const void* const* const			m_postBarriers;
};

ComputeCommand::ComputeCommand (const vk::DeviceInterface&	vki,
								vk::VkDevice				device,
								vk::VkPipeline				pipeline,
								vk::VkPipelineLayout		pipelineLayout,
								const tcu::UVec3&			numWorkGroups,
								int							numDescriptorSets,
								const vk::VkDescriptorSet*	descriptorSets,
								int							numDynamicOffsets,
								const deUint32*				dynamicOffsets,
								int							numPreBarriers,
								const void* const*			preBarriers,
								int							numPostBarriers,
								const void* const*			postBarriers)
	: m_vki					(vki)
	, m_device				(device)
	, m_pipeline			(pipeline)
	, m_pipelineLayout		(pipelineLayout)
	, m_numWorkGroups		(numWorkGroups)
	, m_numDescriptorSets	(numDescriptorSets)
	, m_descriptorSets		(descriptorSets)
	, m_numDynamicOffsets	(numDynamicOffsets)
	, m_dynamicOffsets		(dynamicOffsets)
	, m_numPreBarriers		(numPreBarriers)
	, m_preBarriers			(preBarriers)
	, m_numPostBarriers		(numPostBarriers)
	, m_postBarriers		(postBarriers)
{
}

void ComputeCommand::submitAndWait (deUint32 queueFamilyIndex, vk::VkQueue queue) const
{
	const vk::VkCmdPoolCreateInfo					cmdPoolCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,
		DE_NULL,
		queueFamilyIndex,						// queueFamilyIndex
		vk::VK_CMD_POOL_CREATE_TRANSIENT_BIT,	// flags
	};
	const vk::Unique<vk::VkCmdPool>					cmdPool				(vk::createCommandPool(m_vki, m_device, &cmdPoolCreateInfo));

	const vk::VkFenceCreateInfo						fenceCreateInfo		=
	{
		vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,
		0u,			// flags
	};

	const vk::VkCmdBufferCreateInfo					cmdBufCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
		DE_NULL,
		*cmdPool,							// cmdPool
		vk::VK_CMD_BUFFER_LEVEL_PRIMARY,	// level
		0u,									// flags
	};
	const vk::VkCmdBufferBeginInfo					cmdBufBeginInfo		=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		vk::VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | vk::VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
		(vk::VkRenderPass)0u,																			// renderPass
		0u,																								// subpass
		(vk::VkFramebuffer)0u,																			// framebuffer
	};

	const vk::Unique<vk::VkFence>					cmdCompleteFence	(vk::createFence(m_vki, m_device, &fenceCreateInfo));
	const vk::Unique<vk::VkCmdBuffer>				cmd					(vk::createCommandBuffer(m_vki, m_device, &cmdBufCreateInfo));
	const deUint64									infiniteTimeout		= ~(deUint64)0u;

	VK_CHECK(m_vki.beginCommandBuffer(*cmd, &cmdBufBeginInfo));

	m_vki.cmdBindPipeline(*cmd, vk::VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
	m_vki.cmdBindDescriptorSets(*cmd, vk::VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, m_numDescriptorSets, m_descriptorSets, m_numDynamicOffsets, m_dynamicOffsets);

	if (m_numPreBarriers)
		m_vki.cmdPipelineBarrier(*cmd, 0u, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_FALSE, m_numPreBarriers, m_preBarriers);

	m_vki.cmdDispatch(*cmd, m_numWorkGroups.x(), m_numWorkGroups.y(), m_numWorkGroups.z());
	m_vki.cmdPipelineBarrier(*cmd, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_FALSE, m_numPostBarriers, m_postBarriers);
	VK_CHECK(m_vki.endCommandBuffer(*cmd));

	// run
	VK_CHECK(m_vki.queueSubmit(queue, 1, &cmd.get(), *cmdCompleteFence));
	VK_CHECK(m_vki.waitForFences(m_device, 1, &cmdCompleteFence.get(), 0u, infiniteTimeout)); // \note: timeout is failure
}

class BufferComputeInstance : public vkt::TestInstance
{
public:
											BufferComputeInstance		(Context&				context,
																		 vk::VkDescriptorType	descriptorType,
																		 ShaderInputInterface	shaderInterface,
																		 bool					viewOffset,
																		 bool					dynamicOffset,
																		 bool					dynamicOffsetNonZero);

private:
	vk::Move<vk::VkBuffer>					createColorDataBuffer		(deUint32 offset, deUint32 bufferSize, const tcu::Vec4& value1, const tcu::Vec4& value2, de::MovePtr<vk::Allocation>* outAllocation);
	vk::Move<vk::VkBufferView>				createBufferView			(vk::VkBuffer buffer, deUint32 offset) const;
	vk::Move<vk::VkDescriptorSetLayout>		createDescriptorSetLayout	(void) const;
	vk::Move<vk::VkDescriptorPool>			createDescriptorPool		(void) const;
	vk::Move<vk::VkDescriptorSet>			createDescriptorSet			(vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout, vk::VkBuffer viewA, deUint32 offsetA, vk::VkBuffer viewB, deUint32 offsetB, vk::VkBuffer resBuf) const;

	tcu::TestStatus							iterate						(void);
	void									logTestPlan					(void) const;
	tcu::TestStatus							testResourceAccess			(void);

	enum
	{
		STATIC_OFFSET_VALUE_A	= 256,
		DYNAMIC_OFFSET_VALUE_A	= 512,
		STATIC_OFFSET_VALUE_B	= 1024,
		DYNAMIC_OFFSET_VALUE_B	= 768,
	};

	const vk::VkDescriptorType				m_descriptorType;
	const ShaderInputInterface				m_shaderInterface;
	const bool								m_setViewOffset;
	const bool								m_setDynamicOffset;
	const bool								m_dynamicOffsetNonZero;

	const vk::DeviceInterface&				m_vki;
	const vk::VkDevice						m_device;
	const vk::VkQueue						m_queue;
	const deUint32							m_queueFamilyIndex;
	vk::Allocator&							m_allocator;

	const ComputeInstanceResultBuffer		m_result;
};

BufferComputeInstance::BufferComputeInstance (Context&					context,
											  vk::VkDescriptorType		descriptorType,
											  ShaderInputInterface		shaderInterface,
											  bool						viewOffset,
											  bool						dynamicOffset,
											  bool						dynamicOffsetNonZero)
	: vkt::TestInstance			(context)
	, m_descriptorType			(descriptorType)
	, m_shaderInterface			(shaderInterface)
	, m_setViewOffset			(viewOffset)
	, m_setDynamicOffset		(dynamicOffset)
	, m_dynamicOffsetNonZero	(dynamicOffsetNonZero)
	, m_vki						(context.getDeviceInterface())
	, m_device					(context.getDevice())
	, m_queue					(context.getUniversalQueue())
	, m_queueFamilyIndex		(context.getUniversalQueueFamilyIndex())
	, m_allocator				(context.getDefaultAllocator())
	, m_result					(m_vki, m_device, m_allocator)
{
	if (m_dynamicOffsetNonZero)
		DE_ASSERT(m_setDynamicOffset);
}

vk::Move<vk::VkBuffer> BufferComputeInstance::createColorDataBuffer (deUint32 offset, deUint32 bufferSize, const tcu::Vec4& value1, const tcu::Vec4& value2, de::MovePtr<vk::Allocation>* outAllocation)
{
	DE_ASSERT(offset + sizeof(tcu::Vec4[2]) <= bufferSize);

	const bool						isUniformBuffer		= isUniformDescriptorType(m_descriptorType);
	const vk::VkBufferUsageFlags	usageFlags			= (isUniformBuffer) ? (vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) : (vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	const vk::VkBufferCreateInfo	createInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		(vk::VkDeviceSize)bufferSize,	// size
		usageFlags,						// usage
		0u,								// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,	// sharingMode
		0u,								// queueFamilyCount
		DE_NULL,						// pQueueFamilyIndices
	};
	vk::Move<vk::VkBuffer>			buffer				(vk::createBuffer(m_vki, m_device, &createInfo));
	de::MovePtr<vk::Allocation>		allocation			(allocateAndBindObjectMemory(m_vki, m_device, m_allocator, *buffer, vk::MemoryRequirement::HostVisible));
	void*							mapPtr				= allocation->getHostPtr();

	if (offset)
		deMemset(mapPtr, 0x5A, (size_t)offset);
	deMemcpy((deUint8*)mapPtr + offset, value1.getPtr(), sizeof(tcu::Vec4));
	deMemcpy((deUint8*)mapPtr + offset + sizeof(tcu::Vec4), value2.getPtr(), sizeof(tcu::Vec4));
	deMemset((deUint8*)mapPtr + offset + 2 * sizeof(tcu::Vec4), 0x5A, (size_t)bufferSize - (size_t)offset - 2 * sizeof(tcu::Vec4));

	flushMappedMemoryRange(m_vki, m_device, allocation->getMemory(), allocation->getOffset(), bufferSize);

	*outAllocation = allocation;
	return buffer;
}

vk::Move<vk::VkDescriptorSetLayout> BufferComputeInstance::createDescriptorSetLayout (void) const
{
	vk::DescriptorSetLayoutBuilder builder;

	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);

	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.addArrayBinding(m_descriptorType, 2u, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		default:
			DE_FATAL("Impossible");
	};

	return builder.build(m_vki, m_device);
}

vk::Move<vk::VkDescriptorPool> BufferComputeInstance::createDescriptorPool (void) const
{
	return vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(m_descriptorType, getInterfaceNumResources(m_shaderInterface))
		.build(m_vki, m_device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);
}

vk::Move<vk::VkDescriptorSet> BufferComputeInstance::createDescriptorSet (vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout, vk::VkBuffer viewA, deUint32 offsetA, vk::VkBuffer viewB, deUint32 offsetB, vk::VkBuffer resBuf) const
{
	const vk::VkDescriptorInfo		resultInfo		= createDescriptorInfo(resBuf, 0u, (vk::VkDeviceSize)ComputeInstanceResultBuffer::DATA_SIZE);
	const vk::VkDescriptorInfo		bufferInfos[2]	=
	{
		createDescriptorInfo(viewA, (vk::VkDeviceSize)offsetA, (vk::VkDeviceSize)sizeof(tcu::Vec4[2])),
		createDescriptorInfo(viewB, (vk::VkDeviceSize)offsetB, (vk::VkDeviceSize)sizeof(tcu::Vec4[2])),
	};

	vk::Move<vk::VkDescriptorSet>	descriptorSet	= allocDescriptorSet(m_vki, m_device, pool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);
	vk::DescriptorSetUpdateBuilder	builder;

	// result
	builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo);

	// buffers
	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, &bufferInfos[0]);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, &bufferInfos[0]);
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), m_descriptorType, &bufferInfos[1]);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.writeArray(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, 2u, bufferInfos);
			break;

		default:
			DE_FATAL("Impossible");
	}

	builder.update(m_vki, m_device);
	return descriptorSet;
}

tcu::TestStatus BufferComputeInstance::iterate (void)
{
	logTestPlan();
	return testResourceAccess();
}

void BufferComputeInstance::logTestPlan (void) const
{
	std::ostringstream msg;

	msg << "Accessing resource in a compute program.\n"
		<< "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
				(m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
				(m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
				(const char*)DE_NULL)
		<< " source descriptor(s) of type " << vk::getDescriptorTypeName(m_descriptorType)
		<< " and one destination VK_DESCRIPTOR_TYPE_STORAGE_BUFFER to store results to.\n"
		<< "Source descriptor buffer view(s) have " << ((m_setViewOffset) ? ("non-") : ("")) << "zero offset.\n";

	if (isDynamicDescriptorType(m_descriptorType))
	{
		if (m_setDynamicOffset)
		{
			msg << "Source buffer(s) are given a dynamic offset at bind time.\n"
				<< "The supplied dynamic offset is " << ((m_dynamicOffsetNonZero) ? ("non-") : ("")) << "zero.\n";
		}
		else
		{
			msg << "Dynamic offset is not supplied at bind time. Expecting bind to offset 0.\n";
		}
	}

	msg << "Destination buffer is pre-initialized to -1.\n";

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< msg.str()
		<< tcu::TestLog::EndMessage;
}

tcu::TestStatus BufferComputeInstance::testResourceAccess (void)
{
	enum
	{
		ADDRESSABLE_SIZE = 256, // allocate a lot more than required
	};

	const bool										isDynamicCase		= isDynamicDescriptorType(m_descriptorType);
	const bool										isUniformBuffer		= isUniformDescriptorType(m_descriptorType);
	const deUint32									bindTimeOffsets[]	=
	{
		(m_dynamicOffsetNonZero) ? ((deUint32)DYNAMIC_OFFSET_VALUE_A) : (0u),
		(m_dynamicOffsetNonZero) ? ((deUint32)DYNAMIC_OFFSET_VALUE_B) : (0u),
	};

	const tcu::Vec4									colorA1				= tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4									colorA2				= tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4									colorB1				= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4									colorB2				= tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);

	const deUint32									dataOffsetA			= (isDynamicCase) ? (bindTimeOffsets[0]) : (m_setViewOffset) ? ((deUint32)STATIC_OFFSET_VALUE_A) : (0u);
	const deUint32									dataOffsetB			= (isDynamicCase) ? (bindTimeOffsets[1]) : (m_setViewOffset) ? ((deUint32)STATIC_OFFSET_VALUE_B) : (0u);
	const deUint32									viewOffsetA			= (m_setViewOffset) ? ((deUint32)STATIC_OFFSET_VALUE_A) : (0u);
	const deUint32									viewOffsetB			= (m_setViewOffset) ? ((deUint32)STATIC_OFFSET_VALUE_B) : (0u);
	const deUint32									bufferSizeA			= dataOffsetA + ADDRESSABLE_SIZE;
	const deUint32									bufferSizeB			= dataOffsetB + ADDRESSABLE_SIZE;

	de::MovePtr<vk::Allocation>						bufferMemA;
	const vk::Unique<vk::VkBuffer>					bufferA				(createColorDataBuffer(dataOffsetA, bufferSizeA, colorA1, colorA2, &bufferMemA));

	de::MovePtr<vk::Allocation>						bufferMemB;
	const vk::Unique<vk::VkBuffer>					bufferB				((getInterfaceNumResources(m_shaderInterface) == 1u)
																			? (vk::Move<vk::VkBuffer>())
																			: (createColorDataBuffer(dataOffsetB, bufferSizeB, colorB1, colorB2, &bufferMemB)));

	const vk::Unique<vk::VkDescriptorSetLayout>		descriptorSetLayout	(createDescriptorSetLayout());
	const vk::Unique<vk::VkDescriptorPool>			descriptorPool		(createDescriptorPool());
	const vk::Unique<vk::VkDescriptorSet>			descriptorSet		(createDescriptorSet(*descriptorPool, *descriptorSetLayout, *bufferA, viewOffsetA, *bufferB, viewOffsetB, m_result.getBuffer()));
	const ComputePipeline							pipeline			(m_vki, m_device, m_context.getBinaryCollection(), 1, &descriptorSetLayout.get());

	const vk::VkMemoryInputFlags					inputBit			= (isUniformBuffer) ? (vk::VK_MEMORY_INPUT_UNIFORM_READ_BIT) : (vk::VK_MEMORY_INPUT_SHADER_READ_BIT);
	const vk::VkBufferMemoryBarrier					bufferBarrierA		=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_HOST_WRITE_BIT,		// outputMask
		inputBit,									// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
		*bufferA,									// buffer
		(vk::VkDeviceSize)0u,						// offset
		(vk::VkDeviceSize)bufferSizeA,				// size
	};
	const vk::VkBufferMemoryBarrier					bufferBarrierB		=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_HOST_WRITE_BIT,		// outputMask
		inputBit,									// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
		*bufferB,									// buffer
		(vk::VkDeviceSize)0u,						// offset
		(vk::VkDeviceSize)bufferSizeB,				// size
	};

	const deUint32									numSrcBuffers		= getInterfaceNumResources(m_shaderInterface);

	const vk::VkDescriptorSet						descriptorSets[]	= { *descriptorSet };
	const int										numDescriptorSets	= DE_LENGTH_OF_ARRAY(descriptorSets);
	const deUint32* const							dynamicOffsets		= (m_setDynamicOffset) ? (bindTimeOffsets) : (DE_NULL);
	const deUint32									numDynamicOffsets	= (m_setDynamicOffset) ? (numSrcBuffers) : (0);
	const void* const								preBarriers[]		= { &bufferBarrierA, &bufferBarrierB };
	const int										numPreBarriers		= numSrcBuffers;
	const void* const								postBarriers[]		= { m_result.getResultReadBarrier() };
	const int										numPostBarriers		= DE_LENGTH_OF_ARRAY(postBarriers);

	const ComputeCommand							compute				(m_vki,
																		 m_device,
																		 pipeline.getPipeline(),
																		 pipeline.getPipelineLayout(),
																		 tcu::UVec3(4, 1, 1),
																		 numDescriptorSets,	descriptorSets,
																		 numDynamicOffsets,	dynamicOffsets,
																		 numPreBarriers,	preBarriers,
																		 numPostBarriers,	postBarriers);

	const tcu::Vec4									refQuadrantValue14	= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR)		? (colorA2) :
																		  (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS)	? (colorB2) :
																		  (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY)		? (colorB2) :
																																	(tcu::Vec4(-2.0f));
	const tcu::Vec4									refQuadrantValue23	= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR)		? (colorA1) :
																		  (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS)	? (colorA1) :
																		  (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY)		? (colorA1) :
																																	(tcu::Vec4(-2.0f));
	const tcu::Vec4									references[4]		=
	{
		refQuadrantValue14,
		refQuadrantValue23,
		refQuadrantValue23,
		refQuadrantValue14,
	};
	tcu::Vec4										results[4];

	compute.submitAndWait(m_queueFamilyIndex, m_queue);
	m_result.readResultContentsTo(&results);

	// verify
	if (results[0] == references[0] &&
		results[1] == references[1] &&
		results[2] == references[2] &&
		results[3] == references[3])
	{
		return tcu::TestStatus::pass("Pass");
	}
	else if (results[0] == tcu::Vec4(-1.0f) &&
			 results[1] == tcu::Vec4(-1.0f) &&
			 results[2] == tcu::Vec4(-1.0f) &&
			 results[3] == tcu::Vec4(-1.0f))
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Result buffer was not written to."
			<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Result buffer was not written to");
	}
	else
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Error expected ["
				<< references[0] << ", "
				<< references[1] << ", "
				<< references[2] << ", "
				<< references[3] << "], got ["
				<< results[0] << ", "
				<< results[1] << ", "
				<< results[2] << ", "
				<< results[3] << "]"
			<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Invalid result values");
	}
}

class QuadrantRendederCase : public vkt::TestCase
{
public:
									QuadrantRendederCase		(tcu::TestContext&		testCtx,
																 const char*			name,
																 const char*			description,
																 glu::GLSLVersion		glslVersion,
																 vk::VkShaderStageFlags	exitingStages,
																 vk::VkShaderStageFlags	activeStages);
private:
	virtual std::string				genExtensionDeclarations	(vk::VkShaderStage stage) const = 0;
	virtual std::string				genResourceDeclarations		(vk::VkShaderStage stage, int numUsedBindings) const = 0;
	virtual std::string				genResourceAccessSource		(vk::VkShaderStage stage) const = 0;
	virtual std::string				genNoAccessSource			(void) const = 0;

	std::string						genVertexSource				(void) const;
	std::string						genTessCtrlSource			(void) const;
	std::string						genTessEvalSource			(void) const;
	std::string						genGeometrySource			(void) const;
	std::string						genFragmentSource			(void) const;
	std::string						genComputeSource			(void) const;

	void							initPrograms				(vk::SourceCollections& programCollection) const;

protected:
	const glu::GLSLVersion			m_glslVersion;
	const vk::VkShaderStageFlags	m_exitingStages;
	const vk::VkShaderStageFlags	m_activeStages;
};

QuadrantRendederCase::QuadrantRendederCase (tcu::TestContext&		testCtx,
											const char*				name,
											const char*				description,
											glu::GLSLVersion		glslVersion,
											vk::VkShaderStageFlags	exitingStages,
											vk::VkShaderStageFlags	activeStages)
	: vkt::TestCase		(testCtx, name, description)
	, m_glslVersion		(glslVersion)
	, m_exitingStages	(exitingStages)
	, m_activeStages	(activeStages)
{
	DE_ASSERT((m_exitingStages & m_activeStages) == m_activeStages);
}

std::string QuadrantRendederCase::genVertexSource (void) const
{
	const char* const	nextStageName	= ((m_exitingStages & vk::VK_SHADER_STAGE_TESS_CONTROL_BIT) != 0u)	? ("tsc")
										: ((m_exitingStages & vk::VK_SHADER_STAGE_GEOMETRY_BIT) != 0u)		? ("geo")
										: ((m_exitingStages & vk::VK_SHADER_STAGE_FRAGMENT_BIT) != 0u)		? ("frag")
										: (DE_NULL);
	const char* const	versionDecl		= glu::getGLSLVersionDeclaration(m_glslVersion);
	std::ostringstream	buf;

	if ((m_activeStages & vk::VK_SHADER_STAGE_VERTEX_BIT) != 0u)
	{
		// active vertex shader
		buf << versionDecl << "\n"
			<< genExtensionDeclarations(vk::VK_SHADER_STAGE_VERTEX)
			<< genResourceDeclarations(vk::VK_SHADER_STAGE_VERTEX, 0)
			<< "layout(location = 0) out highp vec4 " << nextStageName << "_color;\n"
			<< "layout(location = 1) flat out highp int " << nextStageName << "_quadrant_id;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	highp vec4 result_position;\n"
			<< "	highp int quadrant_id;\n"
			<< s_quadrantGenVertexPosSource
			<< "	gl_Position = result_position;\n"
			<< "	" << nextStageName << "_quadrant_id = quadrant_id;\n"
			<< "\n"
			<< "	highp vec4 result_color;\n"
			<< genResourceAccessSource(vk::VK_SHADER_STAGE_VERTEX)
			<< "	" << nextStageName << "_color = result_color;\n"
			<< "}\n";
	}
	else
	{
		// do nothing
		buf << versionDecl << "\n"
			<< genExtensionDeclarations(vk::VK_SHADER_STAGE_VERTEX)
			<< "layout(location = 1) flat out highp int " << nextStageName << "_quadrant_id;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	highp vec4 result_position;\n"
			<< "	highp int quadrant_id;\n"
			<< s_quadrantGenVertexPosSource
			<< "	gl_Position = result_position;\n"
			<< "	" << nextStageName << "_quadrant_id = quadrant_id;\n"
			<< "}\n";
	}

	return buf.str();
}

std::string QuadrantRendederCase::genTessCtrlSource (void) const
{
	const char* const	versionDecl		= glu::getGLSLVersionDeclaration(m_glslVersion);
	const bool			extRequired		= glu::glslVersionIsES(m_glslVersion) && m_glslVersion <= glu::GLSL_VERSION_310_ES;
	const char* const	tessExtDecl		= extRequired ? "#extension GL_EXT_tessellation_shader : require\n" : "";
	std::ostringstream	buf;

	if ((m_activeStages & vk::VK_SHADER_STAGE_TESS_CONTROL_BIT) != 0u)
	{
		// contributing not implemented
		DE_ASSERT(m_activeStages == vk::VK_SHADER_STAGE_TESS_CONTROL_BIT);

		// active tc shader
		buf << versionDecl << "\n"
			<< tessExtDecl
			<< genExtensionDeclarations(vk::VK_SHADER_STAGE_TESS_CONTROL)
			<< "layout(vertices=3) out;\n"
			<< genResourceDeclarations(vk::VK_SHADER_STAGE_TESS_CONTROL, 0)
			<< "layout(location = 1) flat in highp int tsc_quadrant_id[];\n"
			<< "layout(location = 0) out highp vec4 tes_color[];\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	highp vec4 result_color;\n"
			<< "	highp int quadrant_id = tsc_quadrant_id[gl_InvocationID];\n"
			<< genResourceAccessSource(vk::VK_SHADER_STAGE_TESS_CONTROL)
			<< "\n"
			<< "	tes_color[gl_InvocationID] = result_color;\n"
			<< "\n"
			<< "	// no dynamic input block indexing\n"
			<< "	highp vec4 position;\n"
			<< "	if (gl_InvocationID == 0)\n"
			<< "		position = gl_in[0].gl_Position;\n"
			<< "	else if (gl_InvocationID == 1)\n"
			<< "		position = gl_in[1].gl_Position;\n"
			<< "	else\n"
			<< "		position = gl_in[2].gl_Position;\n"
			<< "	gl_out[gl_InvocationID].gl_Position = position;\n"
			<< "	gl_TessLevelInner[0] = 2.8;\n"
			<< "	gl_TessLevelInner[1] = 2.8;\n"
			<< "	gl_TessLevelOuter[0] = 2.8;\n"
			<< "	gl_TessLevelOuter[1] = 2.8;\n"
			<< "	gl_TessLevelOuter[2] = 2.8;\n"
			<< "	gl_TessLevelOuter[3] = 2.8;\n"
			<< "}\n";
	}
	else if ((m_activeStages & vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT) != 0u)
	{
		// active te shader, tc passthru
		buf << versionDecl << "\n"
			<< tessExtDecl
			<< "layout(vertices=3) out;\n"
			<< "layout(location = 1) flat in highp int tsc_quadrant_id[];\n"
			<< "layout(location = 1) flat out highp int tes_quadrant_id[];\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	tes_quadrant_id[gl_InvocationID] = tsc_quadrant_id[0];\n"
			<< "\n"
			<< "	// no dynamic input block indexing\n"
			<< "	highp vec4 position;\n"
			<< "	if (gl_InvocationID == 0)\n"
			<< "		position = gl_in[0].gl_Position;\n"
			<< "	else if (gl_InvocationID == 1)\n"
			<< "		position = gl_in[1].gl_Position;\n"
			<< "	else\n"
			<< "		position = gl_in[2].gl_Position;\n"
			<< "	gl_out[gl_InvocationID].gl_Position = position;\n"
			<< "	gl_TessLevelInner[0] = 2.8;\n"
			<< "	gl_TessLevelInner[1] = 2.8;\n"
			<< "	gl_TessLevelOuter[0] = 2.8;\n"
			<< "	gl_TessLevelOuter[1] = 2.8;\n"
			<< "	gl_TessLevelOuter[2] = 2.8;\n"
			<< "	gl_TessLevelOuter[3] = 2.8;\n"
			<< "}\n";
	}
	else
	{
		// passthrough not implemented
		DE_FATAL("not implemented");
	}

	return buf.str();
}

std::string QuadrantRendederCase::genTessEvalSource (void) const
{
	const char* const	versionDecl		= glu::getGLSLVersionDeclaration(m_glslVersion);
	const bool			extRequired		= glu::glslVersionIsES(m_glslVersion) && m_glslVersion <= glu::GLSL_VERSION_310_ES;
	const char* const	tessExtDecl		= extRequired ? "#extension GL_EXT_tessellation_shader : require\n" : "";
	std::ostringstream	buf;

	if ((m_activeStages & vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT) != 0u)
	{
		// contributing not implemented
		DE_ASSERT(m_activeStages == vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT);

		// active te shader
		buf << versionDecl << "\n"
			<< tessExtDecl
			<< genExtensionDeclarations(vk::VK_SHADER_STAGE_TESS_EVALUATION)
			<< "layout(triangles) in;\n"
			<< genResourceDeclarations(vk::VK_SHADER_STAGE_TESS_EVALUATION, 0)
			<< "layout(location = 1) flat in highp int tes_quadrant_id[];\n"
			<< "layout(location = 0) out highp vec4 frag_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	highp vec4 result_color;\n"
			<< "	highp int quadrant_id = tes_quadrant_id[0];\n"
			<< genResourceAccessSource(vk::VK_SHADER_STAGE_TESS_EVALUATION)
			<< "\n"
			<< "	frag_color = result_color;\n"
			<< "	gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position;\n"
			<< "}\n";
	}
	else if ((m_activeStages & vk::VK_SHADER_STAGE_TESS_CONTROL_BIT) != 0u)
	{
		// contributing not implemented
		DE_ASSERT(m_activeStages == vk::VK_SHADER_STAGE_TESS_CONTROL_BIT);

		// active tc shader, te is passthru
		buf << versionDecl << "\n"
			<< tessExtDecl
			<< "layout(triangles) in;\n"
			<< "layout(location = 0) in highp vec4 tes_color[];\n"
			<< "layout(location = 0) out highp vec4 frag_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	frag_color = tes_color[0];\n"
			<< "	gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position;\n"
			<< "}\n";
	}
	else
	{
		// passthrough not implemented
		DE_FATAL("not implemented");
	}

	return buf.str();
}

std::string QuadrantRendederCase::genGeometrySource (void) const
{
	const char* const	versionDecl		= glu::getGLSLVersionDeclaration(m_glslVersion);
	const bool			extRequired		= glu::glslVersionIsES(m_glslVersion) && m_glslVersion <= glu::GLSL_VERSION_310_ES;
	const char* const	geomExtDecl		= extRequired ? "#extension GL_EXT_geometry_shader : require\n" : "";
	std::ostringstream	buf;

	if ((m_activeStages & vk::VK_SHADER_STAGE_GEOMETRY_BIT) != 0u)
	{
		// contributing not implemented
		DE_ASSERT(m_activeStages == vk::VK_SHADER_STAGE_GEOMETRY_BIT);

		// active geometry shader
		buf << versionDecl << "\n"
			<< geomExtDecl
			<< genExtensionDeclarations(vk::VK_SHADER_STAGE_GEOMETRY)
			<< "layout(triangles) in;\n"
			<< "layout(triangle_strip, max_vertices=4) out;\n"
			<< genResourceDeclarations(vk::VK_SHADER_STAGE_GEOMETRY, 0)
			<< "layout(location = 1) flat in highp int geo_quadrant_id[];\n"
			<< "layout(location = 0) out highp vec4 frag_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	highp int quadrant_id;\n"
			<< "	highp vec4 result_color;\n"
			<< "\n"
			<< "	quadrant_id = geo_quadrant_id[0];\n"
			<< genResourceAccessSource(vk::VK_SHADER_STAGE_GEOMETRY)
			<< "	frag_color = result_color;\n"
			<< "	gl_Position = gl_in[0].gl_Position;\n"
			<< "	EmitVertex();\n"
			<< "\n"
			<< "	quadrant_id = geo_quadrant_id[1];\n"
			<< genResourceAccessSource(vk::VK_SHADER_STAGE_GEOMETRY)
			<< "	frag_color = result_color;\n"
			<< "	gl_Position = gl_in[1].gl_Position;\n"
			<< "	EmitVertex();\n"
			<< "\n"
			<< "	quadrant_id = geo_quadrant_id[2];\n"
			<< genResourceAccessSource(vk::VK_SHADER_STAGE_GEOMETRY)
			<< "	frag_color = result_color;\n"
			<< "	gl_Position = gl_in[0].gl_Position * 0.5 + gl_in[2].gl_Position * 0.5;\n"
			<< "	EmitVertex();\n"
			<< "\n"
			<< "	quadrant_id = geo_quadrant_id[0];\n"
			<< genResourceAccessSource(vk::VK_SHADER_STAGE_GEOMETRY)
			<< "	frag_color = result_color;\n"
			<< "	gl_Position = gl_in[2].gl_Position;\n"
			<< "	EmitVertex();\n"
			<< "}\n";
	}
	else
	{
		// passthrough not implemented
		DE_FATAL("not implemented");
	}

	return buf.str();
}

std::string QuadrantRendederCase::genFragmentSource (void) const
{
	const char* const	versionDecl		= glu::getGLSLVersionDeclaration(m_glslVersion);
	std::ostringstream	buf;

	if ((m_activeStages & vk::VK_SHADER_STAGE_FRAGMENT_BIT) != 0u)
	{
		buf << versionDecl << "\n"
			<< genExtensionDeclarations(vk::VK_SHADER_STAGE_GEOMETRY)
			<< genResourceDeclarations(vk::VK_SHADER_STAGE_FRAGMENT, 0);

		if (m_activeStages != vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			// there are other stages, this is just a contributor
			buf << "layout(location = 0) in mediump vec4 frag_color;\n";
		}

		buf << "layout(location = 1) flat in highp int frag_quadrant_id;\n"
			<< "layout(location = 0) out mediump vec4 o_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	highp int quadrant_id = frag_quadrant_id;\n"
			<< "	highp vec4 result_color;\n"
			<< genResourceAccessSource(vk::VK_SHADER_STAGE_FRAGMENT);

		if (m_activeStages != vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			// just contributor
			buf	<< "	if (frag_quadrant_id < 2)\n"
				<< "		o_color = result_color;\n"
				<< "	else\n"
				<< "		o_color = frag_color;\n";
		}
		else
			buf << "	o_color = result_color;\n";

		buf << "}\n";
	}
	else if (m_activeStages == 0u)
	{
		// special case, no active stages
		buf << versionDecl << "\n"
			<< "layout(location = 1) flat in highp int frag_quadrant_id;\n"
			<< "layout(location = 0) out mediump vec4 o_color;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	highp int quadrant_id = frag_quadrant_id;\n"
			<< "	highp vec4 result_color;\n"
			<< genNoAccessSource()
			<< "	o_color = result_color;\n"
			<< "}\n";
	}
	else
	{
		// passthrough
		buf <<	versionDecl << "\n"
			<<	"layout(location = 0) in mediump vec4 frag_color;\n"
				"layout(location = 0) out mediump vec4 o_color;\n"
				"void main (void)\n"
				"{\n"
				"	o_color = frag_color;\n"
				"}\n";
	}

	return buf.str();
}

std::string QuadrantRendederCase::genComputeSource (void) const
{
	const char* const	versionDecl		= glu::getGLSLVersionDeclaration(m_glslVersion);
	std::ostringstream	buf;

	buf	<< versionDecl << "\n"
		<< genExtensionDeclarations(vk::VK_SHADER_STAGE_COMPUTE)
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< genResourceDeclarations(vk::VK_SHADER_STAGE_COMPUTE, 1)
		<< "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
		<< "{\n"
		<< "	highp vec4 read_colors[4];\n"
		<< "} b_out;\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "	highp int quadrant_id = int(gl_WorkGroupID.x);\n"
		<< "	highp vec4 result_color;\n"
		<< genResourceAccessSource(vk::VK_SHADER_STAGE_COMPUTE)
		<< "	b_out.read_colors[gl_WorkGroupID.x] = result_color;\n"
		<< "}\n";

	return buf.str();
}

void QuadrantRendederCase::initPrograms (vk::SourceCollections& programCollection) const
{
	if ((m_exitingStages & vk::VK_SHADER_STAGE_VERTEX_BIT) != 0u)
		programCollection.glslSources.add("vertex") << glu::VertexSource(genVertexSource());

	if ((m_exitingStages & vk::VK_SHADER_STAGE_TESS_CONTROL_BIT) != 0u)
		programCollection.glslSources.add("tess_ctrl") << glu::TessellationControlSource(genTessCtrlSource());

	if ((m_exitingStages & vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT) != 0u)
		programCollection.glslSources.add("tess_eval") << glu::TessellationEvaluationSource(genTessEvalSource());

	if ((m_exitingStages & vk::VK_SHADER_STAGE_GEOMETRY_BIT) != 0u)
		programCollection.glslSources.add("geometry") << glu::GeometrySource(genGeometrySource());

	if ((m_exitingStages & vk::VK_SHADER_STAGE_FRAGMENT_BIT) != 0u)
		programCollection.glslSources.add("fragment") << glu::FragmentSource(genFragmentSource());

	if ((m_exitingStages & vk::VK_SHADER_STAGE_COMPUTE_BIT) != 0u)
		programCollection.glslSources.add("compute") << glu::ComputeSource(genComputeSource());
}

class BufferDescriptorCase : public QuadrantRendederCase
{
public:
	enum
	{
		FLAG_VIEW_OFFSET			= (1u << 1u),
		FLAG_DYNAMIC_OFFSET_ZERO	= (1u << 2u),
		FLAG_DYNAMIC_OFFSET_NONZERO	= (1u << 3u),
	};
	// enum continues where resource flags ends
	DE_STATIC_ASSERT((deUint32)FLAG_VIEW_OFFSET == (deUint32)RESOURCE_FLAG_LAST);

									BufferDescriptorCase		(tcu::TestContext&		testCtx,
																 const char*			name,
																 const char*			description,
																 bool					isPrimaryCmdBuf,
																 vk::VkDescriptorType	descriptorType,
																 vk::VkShaderStageFlags	exitingStages,
																 vk::VkShaderStageFlags	activeStages,
																 ShaderInputInterface	shaderInterface,
																 deUint32				flags);

private:
	std::string						genExtensionDeclarations	(vk::VkShaderStage stage) const;
	std::string						genResourceDeclarations		(vk::VkShaderStage stage, int numUsedBindings) const;
	std::string						genResourceAccessSource		(vk::VkShaderStage stage) const;
	std::string						genNoAccessSource			(void) const;

	vkt::TestInstance*				createInstance				(vkt::Context& context) const;

	const bool						m_viewOffset;
	const bool						m_dynamicOffsetSet;
	const bool						m_dynamicOffsetNonZero;
	const bool						m_isPrimaryCmdBuf;
	const vk::VkDescriptorType		m_descriptorType;
	const ShaderInputInterface		m_shaderInterface;
};

BufferDescriptorCase::BufferDescriptorCase (tcu::TestContext&		testCtx,
											const char*				name,
											const char*				description,
											bool					isPrimaryCmdBuf,
											vk::VkDescriptorType	descriptorType,
											vk::VkShaderStageFlags	exitingStages,
											vk::VkShaderStageFlags	activeStages,
											ShaderInputInterface	shaderInterface,
											deUint32				flags)
	: QuadrantRendederCase		(testCtx, name, description, glu::GLSL_VERSION_310_ES, exitingStages, activeStages)
	, m_viewOffset				((flags & FLAG_VIEW_OFFSET) != 0u)
	, m_dynamicOffsetSet		((flags & (FLAG_DYNAMIC_OFFSET_ZERO | FLAG_DYNAMIC_OFFSET_NONZERO)) != 0u)
	, m_dynamicOffsetNonZero	((flags & FLAG_DYNAMIC_OFFSET_NONZERO) != 0u)
	, m_isPrimaryCmdBuf			(isPrimaryCmdBuf)
	, m_descriptorType			(descriptorType)
	, m_shaderInterface			(shaderInterface)
{
}

std::string BufferDescriptorCase::genExtensionDeclarations (vk::VkShaderStage stage) const
{
	DE_UNREF(stage);
	return "";
}

std::string BufferDescriptorCase::genResourceDeclarations (vk::VkShaderStage stage, int numUsedBindings) const
{
	DE_UNREF(stage);

	const bool			isUniform		= isUniformDescriptorType(m_descriptorType);
	const char* const	storageType		= (isUniform) ? ("uniform") : ("buffer");
	std::ostringstream	buf;

	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			buf	<< "layout(set = 0, binding = " << (numUsedBindings) << ", std140) " << storageType << " BufferName\n"
				<< "{\n"
				<< "	highp vec4 colorA;\n"
				<< "	highp vec4 colorB;\n"
				<< "} b_instance;\n";
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			buf	<< "layout(set = 0, binding = " << (numUsedBindings) << ", std140) " << storageType << " BufferNameA\n"
				<< "{\n"
				<< "	highp vec4 colorA;\n"
				<< "	highp vec4 colorB;\n"
				<< "} b_instanceA;\n"
				<< "layout(set = 0, binding = " << (numUsedBindings+1) << ", std140) " << storageType << " BufferNameB\n"
				<< "{\n"
				<< "	highp vec4 colorA;\n"
				<< "	highp vec4 colorB;\n"
				<< "} b_instanceB;\n";
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			buf	<< "layout(set = 0, binding = " << (numUsedBindings) << ", std140) " << storageType << " BufferName\n"
				<< "{\n"
				<< "	highp vec4 colorA;\n"
				<< "	highp vec4 colorB;\n"
				<< "} b_instances[2];\n";
			break;

		default:
			DE_FATAL("Impossible");
	}

	return buf.str();
}

std::string BufferDescriptorCase::genResourceAccessSource (vk::VkShaderStage stage) const
{
	DE_UNREF(stage);

	std::ostringstream buf;

	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			buf << "	if (quadrant_id == 1 || quadrant_id == 2)\n"
				<< "		result_color = b_instance.colorA;\n"
				<< "	else\n"
				<< "		result_color = b_instance.colorB;\n";
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			buf << "	if (quadrant_id == 1 || quadrant_id == 2)\n"
				<< "		result_color = b_instanceA.colorA;\n"
				<< "	else\n"
				<< "		result_color = b_instanceB.colorB;\n";
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			buf << "	if (quadrant_id == 1 || quadrant_id == 2)\n"
				<< "		result_color = b_instances[0].colorA;\n"
				<< "	else\n"
				<< "		result_color = b_instances[1].colorB;\n";
			break;

		default:
			DE_FATAL("Impossible");
	}

	return buf.str();
}

std::string BufferDescriptorCase::genNoAccessSource (void) const
{
	return "	if (quadrant_id == 1 || quadrant_id == 2)\n"
		   "		result_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		   "	else\n"
		   "		result_color = vec4(1.0, 1.0, 0.0, 1.0);\n";
}

vkt::TestInstance* BufferDescriptorCase::createInstance (vkt::Context& context) const
{
	if (m_exitingStages == vk::VK_SHADER_STAGE_COMPUTE_BIT)
	{
		DE_ASSERT(m_isPrimaryCmdBuf); // secondaries are only valid within renderpass
		return new BufferComputeInstance(context, m_descriptorType, m_shaderInterface, m_viewOffset, m_dynamicOffsetSet, m_dynamicOffsetNonZero);
	}
	else
		return new BufferRenderInstance(context, m_isPrimaryCmdBuf, m_descriptorType, m_activeStages, m_shaderInterface, m_viewOffset, m_dynamicOffsetSet, m_dynamicOffsetNonZero);
}

class ImageInstanceImages
{
public:
										ImageInstanceImages		(const vk::DeviceInterface&		vki,
																 vk::VkDevice					device,
																 deUint32						queueFamilyIndex,
																 vk::VkQueue					queue,
																 vk::Allocator&					allocator,
																 vk::VkDescriptorType			descriptorType,
																 vk::VkImageViewType			viewType,
																 int							numImages,
																 deUint32						baseMipLevel,
																 deUint32						baseArraySlice);

private:
	static vk::Move<vk::VkImage>		createImage				(const vk::DeviceInterface&			vki,
																 vk::VkDevice						device,
																 vk::Allocator&						allocator,
																 vk::VkDescriptorType				descriptorType,
																 vk::VkImageViewType				viewType,
																 const tcu::TextureLevelPyramid&	sourceImage,
																 de::MovePtr<vk::Allocation>*		outAllocation);

	static vk::Move<vk::VkImageView>	createImageView			(const vk::DeviceInterface&			vki,
																 vk::VkDevice						device,
																 vk::VkImageViewType				viewType,
																 const tcu::TextureLevelPyramid&	sourceImage,
																 vk::VkImage						image,
																 deUint32							baseMipLevel,
																 deUint32							baseArraySlice);

	void								populateSourceImage		(tcu::TextureLevelPyramid*			dst,
																 bool								isFirst) const;

	void								uploadImage				(const vk::DeviceInterface&			vki,
																 vk::VkDevice						device,
																 deUint32							queueFamilyIndex,
																 vk::VkQueue						queue,
																 vk::Allocator&						allocator,
																 vk::VkImage						image,
																 const tcu::TextureLevelPyramid&	data);

protected:
	enum
	{
		IMAGE_SIZE		= 64,
		NUM_MIP_LEVELS	= 2,
		ARRAY_SIZE		= 2,
	};

	const vk::VkImageViewType			m_viewType;
	const deUint32						m_baseMipLevel;
	const deUint32						m_baseArraySlice;

	const tcu::TextureFormat			m_imageFormat;
	tcu::TextureLevelPyramid			m_sourceImageA;
	tcu::TextureLevelPyramid			m_sourceImageB;

	de::MovePtr<vk::Allocation>			m_imageMemoryA;
	de::MovePtr<vk::Allocation>			m_imageMemoryB;
	vk::Move<vk::VkImage>				m_imageA;
	vk::Move<vk::VkImage>				m_imageB;
	vk::Move<vk::VkImageView>			m_imageViewA;
	vk::Move<vk::VkImageView>			m_imageViewB;
};

ImageInstanceImages::ImageInstanceImages (const vk::DeviceInterface&	vki,
										  vk::VkDevice					device,
										  deUint32						queueFamilyIndex,
										  vk::VkQueue					queue,
										  vk::Allocator&				allocator,
										  vk::VkDescriptorType			descriptorType,
										  vk::VkImageViewType			viewType,
										  int							numImages,
										  deUint32						baseMipLevel,
										  deUint32						baseArraySlice)
	: m_viewType		(viewType)
	, m_baseMipLevel	(baseMipLevel)
	, m_baseArraySlice	(baseArraySlice)
	, m_imageFormat		(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)
	, m_sourceImageA	(m_imageFormat, NUM_MIP_LEVELS)
	, m_sourceImageB	(m_imageFormat, NUM_MIP_LEVELS)
	, m_imageMemoryA	(DE_NULL)
	, m_imageMemoryB	(DE_NULL)
	, m_imageA			(vk::Move<vk::VkImage>())
	, m_imageB			(vk::Move<vk::VkImage>())
	, m_imageViewA		(vk::Move<vk::VkImageView>())
	, m_imageViewB		(vk::Move<vk::VkImageView>())
{
	DE_ASSERT(numImages == 1 || numImages == 2);

	populateSourceImage(&m_sourceImageA, true);
	m_imageA = createImage(vki, device, allocator, descriptorType, viewType, m_sourceImageA, &m_imageMemoryA);
	m_imageViewA = createImageView(vki, device, viewType, m_sourceImageA, *m_imageA, m_baseMipLevel, m_baseArraySlice);
	uploadImage(vki, device, queueFamilyIndex, queue, allocator, *m_imageA, m_sourceImageA);

	if (numImages == 2)
	{
		populateSourceImage(&m_sourceImageB, false);
		m_imageB = createImage(vki, device, allocator, descriptorType, viewType, m_sourceImageB, &m_imageMemoryB);
		m_imageViewB = createImageView(vki, device, viewType, m_sourceImageB, *m_imageB, m_baseMipLevel, m_baseArraySlice);
		uploadImage(vki, device, queueFamilyIndex, queue, allocator, *m_imageB, m_sourceImageB);
	}
}

vk::Move<vk::VkImage> ImageInstanceImages::createImage (const vk::DeviceInterface&			vki,
														vk::VkDevice						device,
														vk::Allocator&						allocator,
														vk::VkDescriptorType				descriptorType,
														vk::VkImageViewType					viewType,
														const tcu::TextureLevelPyramid&		sourceImage,
														de::MovePtr<vk::Allocation>*		outAllocation)
{
	const tcu::ConstPixelBufferAccess	baseLevel	= sourceImage.getLevel(0);
	const bool							isCube		= (viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE || viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);
	const bool							isStorage	= (descriptorType == vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	const deUint32						readUsage	= (isStorage) ? (vk::VK_IMAGE_USAGE_STORAGE_BIT) : (vk::VK_IMAGE_USAGE_SAMPLED_BIT);
	const deUint32						arraySize	= (viewType == vk::VK_IMAGE_VIEW_TYPE_1D || viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)		? (baseLevel.getHeight())
													: (viewType == vk::VK_IMAGE_VIEW_TYPE_2D || viewType == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY)		? (baseLevel.getDepth())
													: (viewType == vk::VK_IMAGE_VIEW_TYPE_3D)														? (1)
													: (viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE || viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)	? (baseLevel.getDepth()) // cube: numFaces * numLayers
																																					: (0);
	const vk::VkExtent3D				extent		=
	{
		// x
		(deInt32)baseLevel.getWidth(),

		// y
		(viewType == vk::VK_IMAGE_VIEW_TYPE_1D || viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY) ? (1) : (deInt32)baseLevel.getHeight(),

		// z
		(viewType == vk::VK_IMAGE_VIEW_TYPE_3D) ? ((deInt32)baseLevel.getDepth()) : (1),
	};
	const vk::VkImageCreateInfo			createInfo	=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		viewTypeToImageType(viewType),											// imageType
		mapToVkTextureFormat(baseLevel.getFormat()),							// format
		extent,																	// extent
		(deUint32)sourceImage.getNumLevels(),									// mipLevels
		arraySize,																// arraySize
		1,																		// samples
		vk::VK_IMAGE_TILING_OPTIMAL,											// tiling
		readUsage | vk::VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,				// usage
		isCube ? ((deUint32)vk::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) : (0u),	// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,											// sharingMode
		0u,																		// queueFamilyCount
		DE_NULL,																// pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED,											// initialLayout
	};
	vk::Move<vk::VkImage>				image		(vk::createImage(vki, device, &createInfo));

	*outAllocation = allocateAndBindObjectMemory(vki, device, allocator, *image, vk::MemoryRequirement::Any);
	return image;
}

vk::Move<vk::VkImageView> ImageInstanceImages::createImageView (const vk::DeviceInterface&			vki,
																vk::VkDevice						device,
																vk::VkImageViewType					viewType,
																const tcu::TextureLevelPyramid&		sourceImage,
																vk::VkImage							image,
																deUint32							baseMipLevel,
																deUint32							baseArraySlice)
{
	const tcu::ConstPixelBufferAccess	baseLevel			= sourceImage.getLevel(0);
	const deUint32						viewTypeBaseSlice	= (viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE || viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? (6 * baseArraySlice) : (baseArraySlice);
	const deUint32						viewArraySize		= (viewType == vk::VK_IMAGE_VIEW_TYPE_1D)			? (1)
															: (viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)		? (baseLevel.getHeight() - viewTypeBaseSlice)
															: (viewType == vk::VK_IMAGE_VIEW_TYPE_2D)			? (1)
															: (viewType == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY)		? (baseLevel.getDepth() - viewTypeBaseSlice)
															: (viewType == vk::VK_IMAGE_VIEW_TYPE_3D)			? (1)
															: (viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE)			? (6)
															: (viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)	? (baseLevel.getDepth() - viewTypeBaseSlice) // cube: numFaces * numLayers
																												: (0);

	DE_ASSERT(viewArraySize > 0);

	const vk::VkImageSubresourceRange	resourceRange	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
		baseMipLevel,									// baseMipLevel
		sourceImage.getNumLevels() - baseMipLevel,		// mipLevels
		viewTypeBaseSlice,								// baseArraySlice
		viewArraySize,									// arraySize
	};
	const vk::VkImageViewCreateInfo		createInfo		=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		image,											// image
		viewType,										// viewType
		mapToVkTextureFormat(baseLevel.getFormat()),	// format
		{
			vk::VK_CHANNEL_SWIZZLE_R,
			vk::VK_CHANNEL_SWIZZLE_G,
			vk::VK_CHANNEL_SWIZZLE_B,
			vk::VK_CHANNEL_SWIZZLE_A
		},												// channels
		resourceRange,									// subresourceRange
		0u,												// flags
	};
	return vk::createImageView(vki, device, &createInfo);
}

void ImageInstanceImages::populateSourceImage (tcu::TextureLevelPyramid* dst, bool isFirst) const
{
	const int numLevels = dst->getNumLevels();

	for (int level = 0; level < numLevels; ++level)
	{
		const int	width	= IMAGE_SIZE >> level;
		const int	height	= (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D || m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)		? (ARRAY_SIZE)
																																: (IMAGE_SIZE >> level);
		const int	depth	= (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D || m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)		? (1)
							: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D || m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY)		? (ARRAY_SIZE)
							: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE || m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)	? (6 * ARRAY_SIZE)
							: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_3D)															? (IMAGE_SIZE >> level)
																																: (1);

		dst->allocLevel(level, width, height, depth);

		{
			const tcu::PixelBufferAccess levelAccess = dst->getLevel(level);

			for (int z = 0; z < depth; ++z)
			for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x)
			{
				const int			gradPos	= x + y + z;
				const int			gradMax	= width + height + depth - 3;

				const int			red		= 255 * gradPos / gradMax;													//!< gradient from 0 -> max (detects large offset errors)
				const int			green	= ((gradPos % 2 == 0) ? (127) : (0)) + ((gradPos % 4 < 3) ? (128) : (0));	//!< 3-level M pattern (detects small offset errors)
				const int			blue	= (128 * level / numLevels) + (isFirst ? 127 : 0);							//!< level and image index (detects incorrect lod / image)

				DE_ASSERT(de::inRange(red, 0, 255));
				DE_ASSERT(de::inRange(green, 0, 255));
				DE_ASSERT(de::inRange(blue, 0, 255));

				levelAccess.setPixel(tcu::IVec4(red, green, blue, 255), x, y, z);
			}
		}
	}
}

void ImageInstanceImages::uploadImage (const vk::DeviceInterface&		vki,
									   vk::VkDevice						device,
									   deUint32							queueFamilyIndex,
									   vk::VkQueue						queue,
									   vk::Allocator&					allocator,
									   vk::VkImage						image,
									   const tcu::TextureLevelPyramid&	data)
{
	const deUint32						arraySize					= (m_viewType == vk::VK_IMAGE_VIEW_TYPE_3D) ? (1) :
																	  (m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE || m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? (6 * (deUint32)ARRAY_SIZE) :
																	  ((deUint32)ARRAY_SIZE);
	const deUint32						dataBufferSize				= getTextureLevelPyramidDataSize(data);
	const vk::VkBufferCreateInfo		bufferCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		dataBufferSize,										// size
		vk::VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT,			// usage
		0u,													// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,						// sharingMode
		0u,													// queueFamilyCount
		DE_NULL,											// pQueueFamilyIndices
	};
	const vk::Unique<vk::VkBuffer>		dataBuffer					(vk::createBuffer(vki, device, &bufferCreateInfo));
	const de::MovePtr<vk::Allocation>	dataBufferMemory			= allocateAndBindObjectMemory(vki, device, allocator, *dataBuffer, vk::MemoryRequirement::HostVisible);
	const vk::VkFenceCreateInfo			fenceCreateInfo				=
	{
		vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,
		0u,													// flags
	};
	const vk::VkBufferMemoryBarrier		preMemoryBarrier			=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_HOST_WRITE_BIT,				// outputMask
		vk::VK_MEMORY_INPUT_TRANSFER_BIT,					// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,						// destQueueFamilyIndex
		*dataBuffer,										// buffer
		0u,													// offset
		dataBufferSize,										// size
	};
	const vk::VkImageSubresourceRange	fullSubrange				=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,						// aspectMask
		0u,													// baseMipLevel
		(deUint32)data.getNumLevels(),						// mipLevels
		0u,													// baseArraySlice
		arraySize,											// arraySize
	};
	const vk::VkImageMemoryBarrier		preImageBarrier				=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		DE_NULL,
		0u,													// outputMask
		0u,													// inputMask
		vk::VK_IMAGE_LAYOUT_UNDEFINED,						// oldLayout
		vk::VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,	// newLayout
		vk::VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,						// destQueueFamilyIndex
		image,												// image
		fullSubrange										// subresourceRange
	};
	const vk::VkImageMemoryBarrier		postImageBarrier			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_TRANSFER_BIT,					// outputMask
		vk::VK_MEMORY_INPUT_SHADER_READ_BIT,				// inputMask
		vk::VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,	// oldLayout
		vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,		// newLayout
		vk::VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,						// destQueueFamilyIndex
		image,												// image
		fullSubrange										// subresourceRange
	};
	const vk::VkCmdPoolCreateInfo		cmdPoolCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,
		DE_NULL,
		queueFamilyIndex,									// queueFamilyIndex
		vk::VK_CMD_POOL_CREATE_TRANSIENT_BIT,				// flags
	};
	const vk::Unique<vk::VkCmdPool>		cmdPool						(vk::createCommandPool(vki, device, &cmdPoolCreateInfo));
	const vk::VkCmdBufferCreateInfo		cmdBufCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
		DE_NULL,
		*cmdPool,											// cmdPool
		vk::VK_CMD_BUFFER_LEVEL_PRIMARY,					// level
		0u,													// flags
	};
	const vk::VkCmdBufferBeginInfo		cmdBufBeginInfo				=
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		vk::VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | vk::VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
		(vk::VkRenderPass)0u,																			// renderPass
		0u,																								// subpass
		(vk::VkFramebuffer)0u,																			// framebuffer
	};

	const vk::Unique<vk::VkCmdBuffer>	cmd							(vk::createCommandBuffer(vki, device, &cmdBufCreateInfo));
	const void* const					preBarriers[2]				= { &preMemoryBarrier, &preImageBarrier };
	const void* const					postBarriers[1]				= { &postImageBarrier };
	const vk::Unique<vk::VkFence>		cmdCompleteFence			(vk::createFence(vki, device, &fenceCreateInfo));
	const deUint64						infiniteTimeout				= ~(deUint64)0u;
	std::vector<vk::VkBufferImageCopy>	copySlices;

	// copy data to buffer
	writeTextureLevelPyramidData(dataBufferMemory->getHostPtr(), dataBufferSize, data, m_viewType , &copySlices);
	flushMappedMemoryRange(vki, device, dataBufferMemory->getMemory(), dataBufferMemory->getOffset(), dataBufferSize);

	// record command buffer
	VK_CHECK(vki.beginCommandBuffer(*cmd, &cmdBufBeginInfo));
	vki.cmdPipelineBarrier(*cmd, 0u, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_FALSE, DE_LENGTH_OF_ARRAY(preBarriers), preBarriers);
	vki.cmdCopyBufferToImage(*cmd, *dataBuffer, image, vk::VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, (deUint32)copySlices.size(), &copySlices[0]);
	vki.cmdPipelineBarrier(*cmd, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_FALSE, DE_LENGTH_OF_ARRAY(postBarriers), postBarriers);
	VK_CHECK(vki.endCommandBuffer(*cmd));

	// submit and wait for command buffer to complete before killing it
	VK_CHECK(vki.queueSubmit(queue, 1, &cmd.get(), *cmdCompleteFence));
	VK_CHECK(vki.waitForFences(device, 1, &cmdCompleteFence.get(), 0u, infiniteTimeout)); // \note: timeout is failure
}

class ImageFetchInstanceImages : private ImageInstanceImages
{
public:
								ImageFetchInstanceImages	(const vk::DeviceInterface&		vki,
															 vk::VkDevice					device,
															 deUint32						queueFamilyIndex,
															 vk::VkQueue					queue,
															 vk::Allocator&					allocator,
															 vk::VkDescriptorType			descriptorType,
															 ShaderInputInterface			shaderInterface,
															 vk::VkImageViewType			viewType,
															 deUint32						baseMipLevel,
															 deUint32						baseArraySlice);

	static tcu::IVec3			getFetchPos					(vk::VkImageViewType viewType, deUint32 baseMipLevel, deUint32 baseArraySlice, int fetchPosNdx);
	tcu::Vec4					fetchImageValue				(int fetchPosNdx) const;

	inline vk::VkImageView		getImageViewA				(void) const { return *m_imageViewA; }
	inline vk::VkImageView		getImageViewB				(void) const { return *m_imageViewB; }

private:
	enum
	{
		// some arbitrary sample points for all four quadrants
		SAMPLE_POINT_0_X = 6,
		SAMPLE_POINT_0_Y = 13,
		SAMPLE_POINT_0_Z = 49,

		SAMPLE_POINT_1_X = 51,
		SAMPLE_POINT_1_Y = 40,
		SAMPLE_POINT_1_Z = 44,

		SAMPLE_POINT_2_X = 42,
		SAMPLE_POINT_2_Y = 26,
		SAMPLE_POINT_2_Z = 19,

		SAMPLE_POINT_3_X = 25,
		SAMPLE_POINT_3_Y = 25,
		SAMPLE_POINT_3_Z = 18,
	};

	const ShaderInputInterface	m_shaderInterface;
};

ImageFetchInstanceImages::ImageFetchInstanceImages (const vk::DeviceInterface&	vki,
													vk::VkDevice				device,
													deUint32					queueFamilyIndex,
													vk::VkQueue					queue,
													vk::Allocator&				allocator,
													vk::VkDescriptorType		descriptorType,
													ShaderInputInterface		shaderInterface,
													vk::VkImageViewType			viewType,
													deUint32					baseMipLevel,
													deUint32					baseArraySlice)
	: ImageInstanceImages	(vki,
							 device,
							 queueFamilyIndex,
							 queue,
							 allocator,
							 descriptorType,
							 viewType,
							 getInterfaceNumResources(shaderInterface),	// numImages
							 baseMipLevel,
							 baseArraySlice)
	, m_shaderInterface		(shaderInterface)
{
}

bool isImageViewTypeArray (vk::VkImageViewType type)
{
	return type == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY || type == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY || type == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
}

tcu::IVec3 ImageFetchInstanceImages::getFetchPos (vk::VkImageViewType viewType, deUint32 baseMipLevel, deUint32 baseArraySlice, int fetchPosNdx)
{
	const tcu::IVec3	fetchPositions[4]	=
	{
		tcu::IVec3(SAMPLE_POINT_0_X, SAMPLE_POINT_0_Y, SAMPLE_POINT_0_Z),
		tcu::IVec3(SAMPLE_POINT_1_X, SAMPLE_POINT_1_Y, SAMPLE_POINT_1_Z),
		tcu::IVec3(SAMPLE_POINT_2_X, SAMPLE_POINT_2_Y, SAMPLE_POINT_2_Z),
		tcu::IVec3(SAMPLE_POINT_3_X, SAMPLE_POINT_3_Y, SAMPLE_POINT_3_Z),
	};
	const tcu::IVec3	coord				= de::getSizedArrayElement<4>(fetchPositions, fetchPosNdx);
	const deUint32		imageSize			= (deUint32)IMAGE_SIZE >> baseMipLevel;
	const deUint32		arraySize			= isImageViewTypeArray(viewType) ? ARRAY_SIZE - baseArraySlice : 1;

	switch (viewType)
	{
		case vk::VK_IMAGE_VIEW_TYPE_1D:
		case vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY:	return tcu::IVec3(coord.x() % imageSize, coord.y() % arraySize, 0);
		case vk::VK_IMAGE_VIEW_TYPE_2D:
		case vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY:	return tcu::IVec3(coord.x() % imageSize, coord.y() % imageSize, coord.z() % arraySize);
		case vk::VK_IMAGE_VIEW_TYPE_CUBE:
		case vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:	return tcu::IVec3(coord.x() % imageSize, coord.y() % imageSize, coord.z() % (arraySize * 6));
		case vk::VK_IMAGE_VIEW_TYPE_3D:			return tcu::IVec3(coord.x() % imageSize, coord.y() % imageSize, coord.z() % imageSize);
		default:
			DE_FATAL("Impossible");
			return tcu::IVec3();
	}
}

tcu::Vec4 ImageFetchInstanceImages::fetchImageValue (int fetchPosNdx) const
{
	DE_ASSERT(de::inBounds(fetchPosNdx, 0, 4));

	const tcu::TextureLevelPyramid&	fetchSrcA	= m_sourceImageA;
	const tcu::TextureLevelPyramid&	fetchSrcB	= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? (m_sourceImageA) : (m_sourceImageB);
	const tcu::TextureLevelPyramid&	fetchSrc	= ((fetchPosNdx % 2) == 0) ? (fetchSrcA) : (fetchSrcB); // sampling order is ABAB
	const tcu::IVec3				fetchPos	= getFetchPos(m_viewType, m_baseMipLevel, m_baseArraySlice, fetchPosNdx);

	return fetchSrc.getLevel(m_baseMipLevel).getPixel(fetchPos.x(), fetchPos.y(), fetchPos.z());
}

class ImageFetchRenderInstance : public SingleCmdRenderInstance
{
public:
													ImageFetchRenderInstance	(vkt::Context&			context,
																				 bool					isPrimaryCmdBuf,
																				 vk::VkDescriptorType	descriptorType,
																				 vk::VkShaderStageFlags	stageFlags,
																				 ShaderInputInterface	shaderInterface,
																				 vk::VkImageViewType	viewType,
																				 deUint32				baseMipLevel,
																				 deUint32				baseArraySlice);

private:
	static vk::Move<vk::VkDescriptorSetLayout>		createDescriptorSetLayout	(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface,
																				 vk::VkShaderStageFlags		stageFlags);

	static vk::Move<vk::VkPipelineLayout>			createPipelineLayout		(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorSetLayout	descriptorSetLayout);

	static vk::Move<vk::VkDescriptorPool>			createDescriptorPool		(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface);

	static vk::Move<vk::VkDescriptorSet>			createDescriptorSet			(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface,
																				 vk::VkDescriptorSetLayout	layout,
																				 vk::VkDescriptorPool		pool,
																				 vk::VkImageView			viewA,
																				 vk::VkImageView			viewB);

	void											logTestPlan					(void) const;
	vk::VkPipelineLayout							getPipelineLayout			(void) const;
	void											writeDrawCmdBuffer			(vk::VkCmdBuffer cmd) const;
	tcu::TestStatus									verifyResultImage			(const tcu::ConstPixelBufferAccess& result) const;

	enum
	{
		RENDER_SIZE = 128,
	};

	const vk::VkDescriptorType						m_descriptorType;
	const vk::VkShaderStageFlags					m_stageFlags;
	const ShaderInputInterface						m_shaderInterface;
	const vk::VkImageViewType						m_viewType;
	const deUint32									m_baseMipLevel;
	const deUint32									m_baseArraySlice;

	const vk::Unique<vk::VkDescriptorSetLayout>		m_descriptorSetLayout;
	const vk::Unique<vk::VkPipelineLayout>			m_pipelineLayout;
	const ImageFetchInstanceImages					m_images;
	const vk::Unique<vk::VkDescriptorPool>			m_descriptorPool;
	const vk::Unique<vk::VkDescriptorSet>			m_descriptorSet;
};

ImageFetchRenderInstance::ImageFetchRenderInstance	(vkt::Context&			context,
													 bool					isPrimaryCmdBuf,
													 vk::VkDescriptorType	descriptorType,
													 vk::VkShaderStageFlags	stageFlags,
													 ShaderInputInterface	shaderInterface,
													 vk::VkImageViewType	viewType,
													 deUint32				baseMipLevel,
													 deUint32				baseArraySlice)
	: SingleCmdRenderInstance	(context, isPrimaryCmdBuf, tcu::UVec2(RENDER_SIZE, RENDER_SIZE))
	, m_descriptorType			(descriptorType)
	, m_stageFlags				(stageFlags)
	, m_shaderInterface			(shaderInterface)
	, m_viewType				(viewType)
	, m_baseMipLevel			(baseMipLevel)
	, m_baseArraySlice			(baseArraySlice)
	, m_descriptorSetLayout		(createDescriptorSetLayout(m_vki, m_device, m_descriptorType, m_shaderInterface, m_stageFlags))
	, m_pipelineLayout			(createPipelineLayout(m_vki, m_device, *m_descriptorSetLayout))
	, m_images					(m_vki, m_device, m_queueFamilyIndex, m_queue, m_allocator, m_descriptorType, m_shaderInterface, m_viewType, m_baseMipLevel, m_baseArraySlice)
	, m_descriptorPool			(createDescriptorPool(m_vki, m_device, m_descriptorType, m_shaderInterface))
	, m_descriptorSet			(createDescriptorSet(m_vki, m_device, m_descriptorType, m_shaderInterface, *m_descriptorSetLayout, *m_descriptorPool, m_images.getImageViewA(), m_images.getImageViewB()))
{
}

vk::Move<vk::VkDescriptorSetLayout> ImageFetchRenderInstance::createDescriptorSetLayout (const vk::DeviceInterface&		vki,
																						 vk::VkDevice					device,
																						 vk::VkDescriptorType			descriptorType,
																						 ShaderInputInterface			shaderInterface,
																						 vk::VkShaderStageFlags			stageFlags)
{
	vk::DescriptorSetLayoutBuilder builder;

	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.addSingleBinding(descriptorType, stageFlags);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.addSingleBinding(descriptorType, stageFlags);
			builder.addSingleBinding(descriptorType, stageFlags);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.addArrayBinding(descriptorType, 2u, stageFlags);
			break;

		default:
			DE_FATAL("Impossible");
	}

	return builder.build(vki, device);
}

vk::Move<vk::VkPipelineLayout> ImageFetchRenderInstance::createPipelineLayout (const vk::DeviceInterface&	vki,
																			   vk::VkDevice					device,
																			   vk::VkDescriptorSetLayout	descriptorSetLayout)
{
	const vk::VkPipelineLayoutCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		1,						// descriptorSetCount
		&descriptorSetLayout,	// pSetLayouts
		0u,						// pushConstantRangeCount
		DE_NULL,				// pPushConstantRanges
	};
	return vk::createPipelineLayout(vki, device, &createInfo);
}

vk::Move<vk::VkDescriptorPool> ImageFetchRenderInstance::createDescriptorPool (const vk::DeviceInterface&	vki,
																			   vk::VkDevice					device,
																			   vk::VkDescriptorType			descriptorType,
																			   ShaderInputInterface			shaderInterface)
{
	return vk::DescriptorPoolBuilder()
		.addType(descriptorType, getInterfaceNumResources(shaderInterface))
		.build(vki, device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);
}

vk::Move<vk::VkDescriptorSet> ImageFetchRenderInstance::createDescriptorSet (const vk::DeviceInterface&		vki,
																			 vk::VkDevice					device,
																			 vk::VkDescriptorType			descriptorType,
																			 ShaderInputInterface			shaderInterface,
																			 vk::VkDescriptorSetLayout		layout,
																			 vk::VkDescriptorPool			pool,
																			 vk::VkImageView				viewA,
																			 vk::VkImageView				viewB)
{
	const vk::VkDescriptorInfo		imageInfos[2]	=
	{
		createDescriptorInfo(viewA, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		createDescriptorInfo(viewB, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
	};

	vk::Move<vk::VkDescriptorSet>	descriptorSet	= allocDescriptorSet(vki, device, pool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);
	vk::DescriptorSetUpdateBuilder	builder;

	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &imageInfos[0]);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &imageInfos[0]);
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), descriptorType, &imageInfos[1]);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.writeArray(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, 2u, imageInfos);
			break;

		default:
			DE_FATAL("Impossible");
	}

	builder.update(vki, device);
	return descriptorSet;
}

void ImageFetchRenderInstance::logTestPlan (void) const
{
	std::ostringstream msg;

	msg << "Rendering 2x2 grid.\n"
		<< "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
		<< " descriptor(s) of type " << vk::getDescriptorTypeName(m_descriptorType) << "\n"
		<< "Image view type is " << vk::getImageViewTypeName(m_viewType) << "\n";

	if (m_baseMipLevel)
		msg << "Image view base mip level = " << m_baseMipLevel << "\n";
	if (m_baseArraySlice)
		msg << "Image view base array slice = " << m_baseArraySlice << "\n";

	if (m_stageFlags == 0u)
	{
		msg << "Descriptors are not accessed in any shader stage.\n";
	}
	else
	{
		msg << "Color in each cell is fetched using the descriptor(s):\n";

		for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
		{
			msg << "Test sample " << resultNdx << ": fetching at position " << m_images.getFetchPos(m_viewType, m_baseMipLevel, m_baseArraySlice, resultNdx);

			if (m_shaderInterface != SHADER_INPUT_SINGLE_DESCRIPTOR)
			{
				const int srcResourceNdx = (resultNdx % 2); // ABAB source
				msg << " from descriptor " << srcResourceNdx;
			}

			msg << "\n";
		}

		msg << "Descriptors are accessed in {"
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_VERTEX_BIT) != 0)			? (" vertex")			: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_TESS_CONTROL_BIT) != 0)	? (" tess_control")		: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT) != 0)	? (" tess_evaluation")	: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_GEOMETRY_BIT) != 0)		? (" geometry")			: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_FRAGMENT_BIT) != 0)		? (" fragment")			: (""))
			<< " } stages.";
	}

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< msg.str()
		<< tcu::TestLog::EndMessage;
}

vk::VkPipelineLayout ImageFetchRenderInstance::getPipelineLayout (void) const
{
	return *m_pipelineLayout;
}

void ImageFetchRenderInstance::writeDrawCmdBuffer (vk::VkCmdBuffer cmd) const
{
	m_vki.cmdBindDescriptorSets(cmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, getPipelineLayout(), 0, 1, &m_descriptorSet.get(), 0, DE_NULL);
	m_vki.cmdDraw(cmd, 6 * 4, 1, 0, 0); // render four quads (two separate triangles)
}

tcu::TestStatus ImageFetchRenderInstance::verifyResultImage (const tcu::ConstPixelBufferAccess& result) const
{
	const tcu::Vec4		green		(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4		yellow		(1.0f, 1.0f, 0.0f, 1.0f);
	const bool			doFetch		= (m_stageFlags != 0u); // no active stages? Then don't fetch
	const tcu::Vec4		sample0		= (!doFetch) ? (yellow)	: (m_images.fetchImageValue(0));
	const tcu::Vec4		sample1		= (!doFetch) ? (green)	: (m_images.fetchImageValue(1));
	const tcu::Vec4		sample2		= (!doFetch) ? (green)	: (m_images.fetchImageValue(2));
	const tcu::Vec4		sample3		= (!doFetch) ? (yellow)	: (m_images.fetchImageValue(3));
	tcu::Surface		reference	(m_targetSize.x(), m_targetSize.y());

	drawQuadrantReferenceResult(reference.getAccess(), sample0, sample1, sample2, sample3);

	if (!bilinearCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", reference.getAccess(), result, tcu::RGBA(1, 1, 1, 1), tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Image verification failed");
	else
		return tcu::TestStatus::pass("Pass");
}

class ImageFetchComputeInstance : public vkt::TestInstance
{
public:
											ImageFetchComputeInstance	(vkt::Context&			context,
																		 vk::VkDescriptorType	descriptorType,
																		 ShaderInputInterface	shaderInterface,
																		 vk::VkImageViewType	viewType,
																		 deUint32				baseMipLevel,
																		 deUint32				baseArraySlice);

private:
	vk::Move<vk::VkDescriptorSetLayout>		createDescriptorSetLayout	(void) const;
	vk::Move<vk::VkDescriptorPool>			createDescriptorPool		(void) const;
	vk::Move<vk::VkDescriptorSet>			createDescriptorSet			(vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout) const;

	tcu::TestStatus							iterate						(void);
	void									logTestPlan					(void) const;
	tcu::TestStatus							testResourceAccess			(void);

	const vk::VkDescriptorType				m_descriptorType;
	const ShaderInputInterface				m_shaderInterface;
	const vk::VkImageViewType				m_viewType;
	const deUint32							m_baseMipLevel;
	const deUint32							m_baseArraySlice;

	const vk::DeviceInterface&				m_vki;
	const vk::VkDevice						m_device;
	const vk::VkQueue						m_queue;
	const deUint32							m_queueFamilyIndex;
	vk::Allocator&							m_allocator;

	const ComputeInstanceResultBuffer		m_result;
	const ImageFetchInstanceImages			m_images;
};

ImageFetchComputeInstance::ImageFetchComputeInstance (Context&				context,
													  vk::VkDescriptorType	descriptorType,
													  ShaderInputInterface	shaderInterface,
													  vk::VkImageViewType	viewType,
													  deUint32				baseMipLevel,
													  deUint32				baseArraySlice)
	: vkt::TestInstance		(context)
	, m_descriptorType		(descriptorType)
	, m_shaderInterface		(shaderInterface)
	, m_viewType			(viewType)
	, m_baseMipLevel		(baseMipLevel)
	, m_baseArraySlice		(baseArraySlice)
	, m_vki					(context.getDeviceInterface())
	, m_device				(context.getDevice())
	, m_queue				(context.getUniversalQueue())
	, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
	, m_allocator			(context.getDefaultAllocator())
	, m_result				(m_vki, m_device, m_allocator)
	, m_images				(m_vki, m_device, m_queueFamilyIndex, m_queue, m_allocator, m_descriptorType, m_shaderInterface, m_viewType, m_baseMipLevel, m_baseArraySlice)
{
}

vk::Move<vk::VkDescriptorSetLayout> ImageFetchComputeInstance::createDescriptorSetLayout (void) const
{
	vk::DescriptorSetLayoutBuilder builder;

	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);

	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.addArrayBinding(m_descriptorType, 2u, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		default:
			DE_FATAL("Impossible");
	};

	return builder.build(m_vki, m_device);
}

vk::Move<vk::VkDescriptorPool> ImageFetchComputeInstance::createDescriptorPool (void) const
{
	return vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(m_descriptorType, getInterfaceNumResources(m_shaderInterface))
		.build(m_vki, m_device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);
}

vk::Move<vk::VkDescriptorSet> ImageFetchComputeInstance::createDescriptorSet (vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout) const
{
	const vk::VkDescriptorInfo		resultInfo		= createDescriptorInfo(m_result.getBuffer(), 0u, (vk::VkDeviceSize)ComputeInstanceResultBuffer::DATA_SIZE);
	const vk::VkDescriptorInfo		imageInfos[2]	=
	{
		createDescriptorInfo(m_images.getImageViewA(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		createDescriptorInfo(m_images.getImageViewB(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
	};

	vk::Move<vk::VkDescriptorSet>	descriptorSet	= allocDescriptorSet(m_vki, m_device, pool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);
	vk::DescriptorSetUpdateBuilder	builder;

	// result
	builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo);

	// images
	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, &imageInfos[0]);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, &imageInfos[0]);
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), m_descriptorType, &imageInfos[1]);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.writeArray(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, 2u, imageInfos);
			break;

		default:
			DE_FATAL("Impossible");
	}

	builder.update(m_vki, m_device);
	return descriptorSet;
}

tcu::TestStatus ImageFetchComputeInstance::iterate (void)
{
	logTestPlan();
	return testResourceAccess();
}

void ImageFetchComputeInstance::logTestPlan (void) const
{
	std::ostringstream msg;

	msg << "Fetching 4 values from image in compute shader.\n"
		<< "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
		<< " descriptor(s) of type " << vk::getDescriptorTypeName(m_descriptorType) << "\n"
		<< "Image view type is " << vk::getImageViewTypeName(m_viewType) << "\n";

	if (m_baseMipLevel)
		msg << "Image view base mip level = " << m_baseMipLevel << "\n";
	if (m_baseArraySlice)
		msg << "Image view base array slice = " << m_baseArraySlice << "\n";

	for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
	{
		msg << "Test sample " << resultNdx << ": fetch at position " << m_images.getFetchPos(m_viewType, m_baseMipLevel, m_baseArraySlice, resultNdx);

		if (m_shaderInterface != SHADER_INPUT_SINGLE_DESCRIPTOR)
		{
			const int srcResourceNdx = (resultNdx % 2); // ABAB source
			msg << " from descriptor " << srcResourceNdx;
		}

		msg << "\n";
	}

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< msg.str()
		<< tcu::TestLog::EndMessage;
}

tcu::TestStatus ImageFetchComputeInstance::testResourceAccess (void)
{
	const vk::Unique<vk::VkDescriptorSetLayout>		descriptorSetLayout	(createDescriptorSetLayout());
	const vk::Unique<vk::VkDescriptorPool>			descriptorPool		(createDescriptorPool());
	const vk::Unique<vk::VkDescriptorSet>			descriptorSet		(createDescriptorSet(*descriptorPool, *descriptorSetLayout));
	const ComputePipeline							pipeline			(m_vki, m_device, m_context.getBinaryCollection(), 1, &descriptorSetLayout.get());

	const vk::VkDescriptorSet						descriptorSets[]	= { *descriptorSet };
	const int										numDescriptorSets	= DE_LENGTH_OF_ARRAY(descriptorSets);
	const deUint32* const							dynamicOffsets		= DE_NULL;
	const int										numDynamicOffsets	= 0;
	const void*	const*								preBarriers			= DE_NULL;
	const int										numPreBarriers		= 0;
	const void* const								postBarriers[]		= { m_result.getResultReadBarrier() };
	const int										numPostBarriers		= DE_LENGTH_OF_ARRAY(postBarriers);

	const ComputeCommand							compute				(m_vki,
																		 m_device,
																		 pipeline.getPipeline(),
																		 pipeline.getPipelineLayout(),
																		 tcu::UVec3(4, 1, 1),
																		 numDescriptorSets,	descriptorSets,
																		 numDynamicOffsets,	dynamicOffsets,
																		 numPreBarriers,	preBarriers,
																		 numPostBarriers,	postBarriers);

	tcu::Vec4										results[4];
	bool											anyResultSet		= false;
	bool											allResultsOk		= true;

	compute.submitAndWait(m_queueFamilyIndex, m_queue);
	m_result.readResultContentsTo(&results);

	// verify
	for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
	{
		const tcu::Vec4	result				= results[resultNdx];
		const tcu::Vec4	reference			= m_images.fetchImageValue(resultNdx);
		const tcu::Vec4	conversionThreshold	= tcu::Vec4(1.0f / 255.0f);

		if (result != tcu::Vec4(-1.0f))
			anyResultSet = true;

		if (tcu::boolAny(tcu::greaterThan(tcu::abs(result - reference), conversionThreshold)))
		{
			allResultsOk = false;

			m_context.getTestContext().getLog()
				<< tcu::TestLog::Message
				<< "Test sample " << resultNdx << ": Expected " << reference << ", got " << result
				<< tcu::TestLog::EndMessage;
		}
	}

	// read back and verify
	if (allResultsOk)
		return tcu::TestStatus::pass("Pass");
	else if (anyResultSet)
		return tcu::TestStatus::fail("Invalid result values");
	else
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Result buffer was not written to."
			<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Result buffer was not written to");
	}
}

class ImageSampleInstanceImages : private ImageInstanceImages
{
public:
										ImageSampleInstanceImages	(const vk::DeviceInterface&		vki,
																	 vk::VkDevice					device,
																	 deUint32						queueFamilyIndex,
																	 vk::VkQueue					queue,
																	 vk::Allocator&					allocator,
																	 vk::VkDescriptorType			descriptorType,
																	 ShaderInputInterface			shaderInterface,
																	 vk::VkImageViewType			viewType,
																	 deUint32						baseMipLevel,
																	 deUint32						baseArraySlice,
																	 bool							immutable);

	static tcu::Vec4					getSamplePos				(vk::VkImageViewType viewType, deUint32 baseMipLevel, deUint32 baseArraySlice, int samplePosNdx);
	tcu::Vec4							fetchSampleValue			(int samplePosNdx) const;

	inline vk::VkImageView				getImageViewA				(void) const { return *m_imageViewA;	}
	inline vk::VkImageView				getImageViewB				(void) const { return *m_imageViewB;	}
	inline vk::VkSampler				getSamplerA					(void) const { return *m_samplerA;		}
	inline vk::VkSampler				getSamplerB					(void) const { return *m_samplerB;		}
	inline bool							isImmutable					(void) const { return m_isImmutable;	}

private:
	static int							getNumImages				(vk::VkDescriptorType descriptorType, ShaderInputInterface shaderInterface);
	static tcu::Sampler					createRefSampler			(bool isFirst);
	static vk::Move<vk::VkSampler>		createSampler				(const vk::DeviceInterface& vki, vk::VkDevice device, const tcu::Sampler& sampler, const tcu::TextureFormat& format);

	static tcu::Texture1DArrayView		getRef1DView				(const tcu::TextureLevelPyramid& source, deUint32 baseMipLevel, deUint32 baseArraySlice, std::vector<tcu::ConstPixelBufferAccess>* levelStorage);
	static tcu::Texture2DArrayView		getRef2DView				(const tcu::TextureLevelPyramid& source, deUint32 baseMipLevel, deUint32 baseArraySlice, std::vector<tcu::ConstPixelBufferAccess>* levelStorage);
	static tcu::Texture3DView			getRef3DView				(const tcu::TextureLevelPyramid& source, deUint32 baseMipLevel, deUint32 baseArraySlice, std::vector<tcu::ConstPixelBufferAccess>* levelStorage);
	static tcu::TextureCubeArrayView	getRefCubeView				(const tcu::TextureLevelPyramid& source, deUint32 baseMipLevel, deUint32 baseArraySlice, std::vector<tcu::ConstPixelBufferAccess>* levelStorage);

	const vk::VkDescriptorType			m_descriptorType;
	const ShaderInputInterface			m_shaderInterface;
	const bool							m_isImmutable;

	const tcu::Sampler					m_refSamplerA;
	const tcu::Sampler					m_refSamplerB;
	const vk::Unique<vk::VkSampler>		m_samplerA;
	const vk::Unique<vk::VkSampler>		m_samplerB;
};

ImageSampleInstanceImages::ImageSampleInstanceImages (const vk::DeviceInterface&	vki,
													  vk::VkDevice					device,
													  deUint32						queueFamilyIndex,
													  vk::VkQueue					queue,
													  vk::Allocator&				allocator,
													  vk::VkDescriptorType			descriptorType,
													  ShaderInputInterface			shaderInterface,
													  vk::VkImageViewType			viewType,
													  deUint32						baseMipLevel,
													  deUint32						baseArraySlice,
													  bool							immutable)
	: ImageInstanceImages	(vki,
							 device,
							 queueFamilyIndex,
							 queue,
							 allocator,
							 descriptorType,
							 viewType,
							 getNumImages(descriptorType, shaderInterface),
							 baseMipLevel,
							 baseArraySlice)
	, m_descriptorType		(descriptorType)
	, m_shaderInterface		(shaderInterface)
	, m_isImmutable			(immutable)
	, m_refSamplerA			(createRefSampler(true))
	, m_refSamplerB			(createRefSampler(false))
	, m_samplerA			(createSampler(vki, device, m_refSamplerA, m_imageFormat))
	, m_samplerB			((getInterfaceNumResources(m_shaderInterface) == 1u)
								? vk::Move<vk::VkSampler>()
								: createSampler(vki, device, m_refSamplerB, m_imageFormat))
{
}

tcu::Vec4 ImageSampleInstanceImages::getSamplePos (vk::VkImageViewType viewType, deUint32 baseMipLevel, deUint32 baseArraySlice, int samplePosNdx)
{
	DE_ASSERT(de::inBounds(samplePosNdx, 0, 4));

	const deUint32	imageSize	= (deUint32)IMAGE_SIZE >> baseMipLevel;
	const deUint32	arraySize	= isImageViewTypeArray(viewType) ? ARRAY_SIZE - baseArraySlice : 1;

	// choose arbitrary values that are not ambiguous with NEAREST filtering

	switch (viewType)
	{
		case vk::VK_IMAGE_VIEW_TYPE_1D:
		case vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		case vk::VK_IMAGE_VIEW_TYPE_2D:
		case vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case vk::VK_IMAGE_VIEW_TYPE_3D:
		{
			const tcu::Vec3	coords[4]	=
			{
				tcu::Vec3(0.75f,
						  0.5f,
						  (float)(12u % imageSize) + 0.25f),

				tcu::Vec3((float)(23u % imageSize) + 0.25f,
						  (float)(73u % imageSize) + 0.5f,
						  (float)(16u % imageSize) + 0.5f + (float)imageSize),

				tcu::Vec3(-(float)(43u % imageSize) + 0.25f,
						  (float)(84u % imageSize) + 0.5f + (float)imageSize,
						  (float)(117u % imageSize) + 0.75f),

				tcu::Vec3((float)imageSize + 0.5f,
						  (float)(75u % imageSize) + 0.25f,
						  (float)(83u % imageSize) + 0.25f + (float)imageSize),
			};
			const deUint32	slices[4]	=
			{
				0u % arraySize,
				4u % arraySize,
				9u % arraySize,
				2u % arraySize,
			};

			if (viewType == vk::VK_IMAGE_VIEW_TYPE_1D || viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)
				return tcu::Vec4(coords[samplePosNdx].x() / (float)imageSize,
								 (float)slices[samplePosNdx],
								 0.0f,
								 0.0f);
			else if (viewType == vk::VK_IMAGE_VIEW_TYPE_2D || viewType == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY)
				return tcu::Vec4(coords[samplePosNdx].x() / (float)imageSize,
								 coords[samplePosNdx].y() / (float)imageSize,
								 (float)slices[samplePosNdx],
								 0.0f);
			else if (viewType == vk::VK_IMAGE_VIEW_TYPE_3D)
				return tcu::Vec4(coords[samplePosNdx].x() / (float)imageSize,
								 coords[samplePosNdx].y() / (float)imageSize,
								 coords[samplePosNdx].z() / (float)imageSize,
								 0.0f);
			else
			{
				DE_FATAL("Impossible");
				return tcu::Vec4();
			}
		}

		case vk::VK_IMAGE_VIEW_TYPE_CUBE:
		case vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		{
			// \note these values are in [0, texSize]*3 space for convenience
			const tcu::Vec3	coords[4]	=
			{
				tcu::Vec3(0.75f,
						  0.5f,
						  (float)imageSize),

				tcu::Vec3((float)(13u % imageSize) + 0.25f,
						  0.0f,
						  (float)(16u % imageSize) + 0.5f),

				tcu::Vec3(0.0f,
						  (float)(84u % imageSize) + 0.5f,
						  (float)(10u % imageSize) + 0.75f),

				tcu::Vec3((float)imageSize,
						  (float)(75u % imageSize) + 0.25f,
						  (float)(83u % imageSize) + 0.75f),
			};
			const deUint32	slices[4]	=
			{
				1u % arraySize,
				2u % arraySize,
				9u % arraySize,
				5u % arraySize,
			};

			DE_ASSERT(de::inRange(coords[samplePosNdx].x(), 0.0f, (float)imageSize));
			DE_ASSERT(de::inRange(coords[samplePosNdx].y(), 0.0f, (float)imageSize));
			DE_ASSERT(de::inRange(coords[samplePosNdx].z(), 0.0f, (float)imageSize));

			// map to [-1, 1]*3 space
			return tcu::Vec4(coords[samplePosNdx].x() / (float)imageSize * 2.0f - 1.0f,
							 coords[samplePosNdx].y() / (float)imageSize * 2.0f - 1.0f,
							 coords[samplePosNdx].z() / (float)imageSize * 2.0f - 1.0f,
							 (float)slices[samplePosNdx]);
		}

		default:
			DE_FATAL("Impossible");
			return tcu::Vec4();
	}
}

tcu::Vec4 ImageSampleInstanceImages::fetchSampleValue (int samplePosNdx) const
{
	DE_ASSERT(de::inBounds(samplePosNdx, 0, 4));

	// texture order is ABAB
	const bool									isSamplerCase	= (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER);
	const tcu::TextureLevelPyramid&				sampleSrcA		= m_sourceImageA;
	const tcu::TextureLevelPyramid&				sampleSrcB		= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? (m_sourceImageA) : (m_sourceImageB);
	const tcu::TextureLevelPyramid&				sampleSrc		= (isSamplerCase) ? (sampleSrcA) : ((samplePosNdx % 2) == 0) ? (sampleSrcA) : (sampleSrcB);

	// sampler order is ABAB
	const tcu::Sampler&							samplerA		= m_refSamplerA;
	const tcu::Sampler&							samplerB		= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? (m_refSamplerA) : (m_refSamplerB);
	const tcu::Sampler&							sampler			= ((samplePosNdx % 2) == 0) ? (samplerA) : (samplerB);

	const tcu::Vec4								samplePos		= getSamplePos(m_viewType, m_baseMipLevel, m_baseArraySlice, samplePosNdx);
	const float									lod				= 0.0f;
	std::vector<tcu::ConstPixelBufferAccess>	levelStorage;

	switch (m_viewType)
	{
		case vk::VK_IMAGE_VIEW_TYPE_1D:
		case vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY:	return getRef1DView(sampleSrc, m_baseMipLevel, m_baseArraySlice, &levelStorage).sample(sampler, samplePos.x(), samplePos.y(), lod);
		case vk::VK_IMAGE_VIEW_TYPE_2D:
		case vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY:	return getRef2DView(sampleSrc, m_baseMipLevel, m_baseArraySlice, &levelStorage).sample(sampler, samplePos.x(), samplePos.y(), samplePos.z(), lod);
		case vk::VK_IMAGE_VIEW_TYPE_3D:			return getRef3DView(sampleSrc, m_baseMipLevel, m_baseArraySlice, &levelStorage).sample(sampler, samplePos.x(), samplePos.y(), samplePos.z(), lod);
		case vk::VK_IMAGE_VIEW_TYPE_CUBE:
		case vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:	return getRefCubeView(sampleSrc, m_baseMipLevel, m_baseArraySlice, &levelStorage).sample(sampler, samplePos.x(), samplePos.y(), samplePos.z(), samplePos.w(), lod);

		default:
		{
			DE_FATAL("Impossible");
			return tcu::Vec4();
		}
	}
}

int ImageSampleInstanceImages::getNumImages (vk::VkDescriptorType descriptorType, ShaderInputInterface shaderInterface)
{
	// If we are testing separate samplers, just one image is enough
	if (descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
		return 1;
	else if (descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		// combined: numImages == numSamplers
		return getInterfaceNumResources(shaderInterface);
	}
	else
	{
		DE_FATAL("Impossible");
		return 0;
	}
}

tcu::Sampler ImageSampleInstanceImages::createRefSampler (bool isFirst)
{
	if (isFirst)
	{
		// linear, wrapping
		return tcu::Sampler(tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::LINEAR, tcu::Sampler::LINEAR);
	}
	else
	{
		// nearest, clamping
		return tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST);
	}
}

vk::Move<vk::VkSampler> ImageSampleInstanceImages::createSampler (const vk::DeviceInterface& vki, vk::VkDevice device, const tcu::Sampler& sampler, const tcu::TextureFormat& format)
{
	const bool						compareEnabled	= (sampler.compare != tcu::Sampler::COMPAREMODE_NONE);
	const vk::VkCompareOp			compareOp		= (compareEnabled) ? (mapToVkCompareOp(sampler.compare)) : (vk::VK_COMPARE_OP_ALWAYS);
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);
	const bool						isIntTexture	= channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER || channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
	const vk::VkBorderColor			borderColor		= (isIntTexture) ? (vk::VK_BORDER_COLOR_INT_OPAQUE_WHITE) : (vk::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
	const vk::VkSamplerCreateInfo	createInfo		=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		DE_NULL,
		mapMagFilterToVkTexFilter(sampler.magFilter),									// magFilter
		mapMinFilterToVkTexFilter(sampler.minFilter),									// minFilter
		mapMinFilterToVkTexMipmapMode(sampler.minFilter),								// mipMode
		mapToVkTexAddressMode(sampler.wrapS),											// addressU
		mapToVkTexAddressMode(sampler.wrapT),											// addressV
		mapToVkTexAddressMode(sampler.wrapR),											// addressW
		0.0f,																			// mipLodBias
		1,																				// maxAnisotropy
		(compareEnabled) ? (vk::VkBool32)(vk::VK_TRUE) : (vk::VkBool32)(vk::VK_FALSE),	// compareEnable
		compareOp,																		// compareOp
		0.0f,																			// minLod
		0.0f,																			// maxLod
		borderColor,																	// borderColor
		vk::VK_FALSE,																	// unnormalizedCoords
	};
	return vk::createSampler(vki, device, &createInfo);
}

tcu::Texture1DArrayView ImageSampleInstanceImages::getRef1DView (const tcu::TextureLevelPyramid& source, deUint32 baseMipLevel, deUint32 baseArraySlice, std::vector<tcu::ConstPixelBufferAccess>* levelStorage)
{
	DE_ASSERT(levelStorage->empty());

	const deUint32 numSlices = (deUint32)source.getLevel(0).getHeight();
	const deUint32 numLevels = (deUint32)source.getNumLevels();

	// cut pyramid from baseMipLevel
	for (deUint32 level = baseMipLevel; level < numLevels; ++level)
	{
		// cut levels from baseArraySlice
		const tcu::ConstPixelBufferAccess wholeLevel	= source.getLevel(level);
		const tcu::ConstPixelBufferAccess cutLevel		= tcu::getSubregion(wholeLevel, 0, baseArraySlice, wholeLevel.getWidth(), numSlices - baseArraySlice);
		levelStorage->push_back(cutLevel);
	}

	return tcu::Texture1DArrayView((int)levelStorage->size(), &levelStorage->front());
}

tcu::Texture2DArrayView ImageSampleInstanceImages::getRef2DView (const tcu::TextureLevelPyramid& source, deUint32 baseMipLevel, deUint32 baseArraySlice, std::vector<tcu::ConstPixelBufferAccess>* levelStorage)
{
	DE_ASSERT(levelStorage->empty());

	const deUint32 numSlices = (deUint32)source.getLevel(0).getDepth();
	const deUint32 numLevels = (deUint32)source.getNumLevels();

	// cut pyramid from baseMipLevel
	for (deUint32 level = baseMipLevel; level < numLevels; ++level)
	{
		// cut levels from baseArraySlice
		const tcu::ConstPixelBufferAccess wholeLevel	= source.getLevel(level);
		const tcu::ConstPixelBufferAccess cutLevel		= tcu::getSubregion(wholeLevel, 0, 0, baseArraySlice, wholeLevel.getWidth(), wholeLevel.getHeight(), numSlices - baseArraySlice);
		levelStorage->push_back(cutLevel);
	}

	return tcu::Texture2DArrayView((int)levelStorage->size(), &levelStorage->front());
}

tcu::Texture3DView ImageSampleInstanceImages::getRef3DView (const tcu::TextureLevelPyramid& source, deUint32 baseMipLevel, deUint32 baseArraySlice, std::vector<tcu::ConstPixelBufferAccess>* levelStorage)
{
	DE_ASSERT(levelStorage->empty());
	DE_ASSERT(baseArraySlice == 0);
	DE_UNREF(baseArraySlice);

	const deUint32 numLevels = (deUint32)source.getNumLevels();

	// cut pyramid from baseMipLevel
	for (deUint32 level = baseMipLevel; level < numLevels; ++level)
		levelStorage->push_back(source.getLevel(level));

	return tcu::Texture3DView((int)levelStorage->size(), &levelStorage->front());
}

tcu::TextureCubeArrayView ImageSampleInstanceImages::getRefCubeView (const tcu::TextureLevelPyramid& source, deUint32 baseMipLevel, deUint32 baseArraySlice, std::vector<tcu::ConstPixelBufferAccess>* levelStorage)
{
	DE_ASSERT(levelStorage->empty());

	const deUint32 numSlices = (deUint32)source.getLevel(0).getDepth() / 6;
	const deUint32 numLevels = (deUint32)source.getNumLevels();

	// cut pyramid from baseMipLevel
	for (deUint32 level = baseMipLevel; level < numLevels; ++level)
	{
		// cut levels from baseArraySlice
		const tcu::ConstPixelBufferAccess wholeLevel	= source.getLevel(level);
		const tcu::ConstPixelBufferAccess cutLevel		= tcu::getSubregion(wholeLevel, 0, 0, baseArraySlice * 6, wholeLevel.getWidth(), wholeLevel.getHeight(), (numSlices - baseArraySlice) * 6);
		levelStorage->push_back(cutLevel);
	}

	return tcu::TextureCubeArrayView((int)levelStorage->size(), &levelStorage->front());
}

class ImageSampleRenderInstance : public SingleCmdRenderInstance
{
public:
													ImageSampleRenderInstance		(vkt::Context&			context,
																					 bool					isPrimaryCmdBuf,
																					 vk::VkDescriptorType	descriptorType,
																					 vk::VkShaderStageFlags	stageFlags,
																					 ShaderInputInterface	shaderInterface,
																					 vk::VkImageViewType	viewType,
																					 deUint32				baseMipLevel,
																					 deUint32				baseArraySlice,
																					 bool					isImmutable);

private:
	static vk::Move<vk::VkDescriptorSetLayout>		createDescriptorSetLayout		(const vk::DeviceInterface&			vki,
																					 vk::VkDevice						device,
																					 vk::VkDescriptorType				descriptorType,
																					 ShaderInputInterface				shaderInterface,
																					 vk::VkShaderStageFlags				stageFlags,
																					 const ImageSampleInstanceImages&	images);

	static vk::Move<vk::VkPipelineLayout>			createPipelineLayout			(const vk::DeviceInterface&	vki,
																					 vk::VkDevice				device,
																					 vk::VkDescriptorSetLayout	descriptorSetLayout);

	static vk::Move<vk::VkDescriptorPool>			createDescriptorPool			(const vk::DeviceInterface&	vki,
																					 vk::VkDevice				device,
																					 vk::VkDescriptorType		descriptorType,
																					 ShaderInputInterface		shaderInterface,
																					 bool						isImmutable);

	static vk::Move<vk::VkDescriptorSet>			createDescriptorSet				(const vk::DeviceInterface&			vki,
																					 vk::VkDevice						device,
																					 vk::VkDescriptorType				descriptorType,
																					 ShaderInputInterface				shaderInterface,
																					 vk::VkDescriptorSetLayout			layout,
																					 vk::VkDescriptorPool				pool,
																					 bool								isImmutable,
																					 const ImageSampleInstanceImages&	images);

	static void										writeSamplerDescriptorSet		(const vk::DeviceInterface&			vki,
																					 vk::VkDevice						device,
																					 ShaderInputInterface				shaderInterface,
																					 bool								isImmutable,
																					 const ImageSampleInstanceImages&	images,
																					 vk::VkDescriptorSet				descriptorSet);

	static void										writeImageSamplerDescriptorSet	(const vk::DeviceInterface&			vki,
																					 vk::VkDevice						device,
																					 ShaderInputInterface				shaderInterface,
																					 bool								isImmutable,
																					 const ImageSampleInstanceImages&	images,
																					 vk::VkDescriptorSet				descriptorSet);

	void											logTestPlan						(void) const;
	vk::VkPipelineLayout							getPipelineLayout				(void) const;
	void											writeDrawCmdBuffer				(vk::VkCmdBuffer cmd) const;
	tcu::TestStatus									verifyResultImage				(const tcu::ConstPixelBufferAccess& result) const;

	enum
	{
		RENDER_SIZE = 128,
	};

	const vk::VkDescriptorType						m_descriptorType;
	const vk::VkShaderStageFlags					m_stageFlags;
	const ShaderInputInterface						m_shaderInterface;
	const vk::VkImageViewType						m_viewType;
	const deUint32									m_baseMipLevel;
	const deUint32									m_baseArraySlice;

	const ImageSampleInstanceImages					m_images;
	const vk::Unique<vk::VkDescriptorSetLayout>		m_descriptorSetLayout;
	const vk::Unique<vk::VkPipelineLayout>			m_pipelineLayout;
	const vk::Unique<vk::VkDescriptorPool>			m_descriptorPool;
	const vk::Unique<vk::VkDescriptorSet>			m_descriptorSet;
};

ImageSampleRenderInstance::ImageSampleRenderInstance (vkt::Context&				context,
													  bool						isPrimaryCmdBuf,
													  vk::VkDescriptorType		descriptorType,
													  vk::VkShaderStageFlags	stageFlags,
													  ShaderInputInterface		shaderInterface,
													  vk::VkImageViewType		viewType,
													  deUint32					baseMipLevel,
													  deUint32					baseArraySlice,
													  bool						isImmutable)
	: SingleCmdRenderInstance	(context, isPrimaryCmdBuf, tcu::UVec2(RENDER_SIZE, RENDER_SIZE))
	, m_descriptorType			(descriptorType)
	, m_stageFlags				(stageFlags)
	, m_shaderInterface			(shaderInterface)
	, m_viewType				(viewType)
	, m_baseMipLevel			(baseMipLevel)
	, m_baseArraySlice			(baseArraySlice)
	, m_images					(m_vki, m_device, m_queueFamilyIndex, m_queue, m_allocator, m_descriptorType, m_shaderInterface, m_viewType, m_baseMipLevel, m_baseArraySlice, isImmutable)
	, m_descriptorSetLayout		(createDescriptorSetLayout(m_vki, m_device, m_descriptorType, m_shaderInterface, m_stageFlags, m_images))
	, m_pipelineLayout			(createPipelineLayout(m_vki, m_device, *m_descriptorSetLayout))
	, m_descriptorPool			(createDescriptorPool(m_vki, m_device, m_descriptorType, m_shaderInterface, isImmutable))
	, m_descriptorSet			(createDescriptorSet(m_vki, m_device, m_descriptorType, m_shaderInterface, *m_descriptorSetLayout, *m_descriptorPool, isImmutable, m_images))
{
}

vk::Move<vk::VkDescriptorSetLayout> ImageSampleRenderInstance::createDescriptorSetLayout (const vk::DeviceInterface&		vki,
																						  vk::VkDevice						device,
																						  vk::VkDescriptorType				descriptorType,
																						  ShaderInputInterface				shaderInterface,
																						  vk::VkShaderStageFlags			stageFlags,
																						  const ImageSampleInstanceImages&	images)
{
	const vk::VkSampler				samplers[2] =
	{
		images.getSamplerA(),
		images.getSamplerB(),
	};

	vk::DescriptorSetLayoutBuilder	builder;

	// with samplers, separate texture at binding 0
	if (descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
		builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stageFlags);

	// (combined)samplers follow
	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.addSingleSamplerBinding(descriptorType, stageFlags, (images.isImmutable()) ? (&samplers[0]) : (DE_NULL));
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.addSingleSamplerBinding(descriptorType, stageFlags, (images.isImmutable()) ? (&samplers[0]) : (DE_NULL));
			builder.addSingleSamplerBinding(descriptorType, stageFlags, (images.isImmutable()) ? (&samplers[1]) : (DE_NULL));
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.addArraySamplerBinding(descriptorType, 2u, stageFlags, (images.isImmutable()) ? (samplers) : (DE_NULL));
			break;

		default:
			DE_FATAL("Impossible");
	}

	return builder.build(vki, device);
}

vk::Move<vk::VkPipelineLayout> ImageSampleRenderInstance::createPipelineLayout (const vk::DeviceInterface&	vki,
																				vk::VkDevice				device,
																				vk::VkDescriptorSetLayout	descriptorSetLayout)
{
	const vk::VkPipelineLayoutCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		1,						// descriptorSetCount
		&descriptorSetLayout,	// pSetLayouts
		0u,						// pushConstantRangeCount
		DE_NULL,				// pPushConstantRanges
	};
	return vk::createPipelineLayout(vki, device, &createInfo);
}

vk::Move<vk::VkDescriptorPool> ImageSampleRenderInstance::createDescriptorPool (const vk::DeviceInterface&	vki,
																				vk::VkDevice				device,
																				vk::VkDescriptorType		descriptorType,
																				ShaderInputInterface		shaderInterface,
																				bool						isImmutable)
{
	vk::DescriptorPoolBuilder builder;

	if (descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
	{
		// separate samplers need image to sample
		builder.addType(vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

		// samplers needed only if they are specified in the descriptor set
		if (!isImmutable)
			builder.addType(vk::VK_DESCRIPTOR_TYPE_SAMPLER, getInterfaceNumResources(shaderInterface));
	}
	else if (descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		// combined image samplers
		builder.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, getInterfaceNumResources(shaderInterface));
	}
	else
		DE_FATAL("Impossible");

	return builder.build(vki, device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);
}

vk::Move<vk::VkDescriptorSet> ImageSampleRenderInstance::createDescriptorSet (const vk::DeviceInterface&		vki,
																			  vk::VkDevice						device,
																			  vk::VkDescriptorType				descriptorType,
																			  ShaderInputInterface				shaderInterface,
																			  vk::VkDescriptorSetLayout			layout,
																			  vk::VkDescriptorPool				pool,
																			  bool								isImmutable,
																			  const ImageSampleInstanceImages&	images)
{
	vk::Move<vk::VkDescriptorSet> descriptorSet = allocDescriptorSet(vki, device, pool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);

	if (descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
		writeSamplerDescriptorSet(vki, device,  shaderInterface, isImmutable, images, *descriptorSet);
	else if (descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		writeImageSamplerDescriptorSet(vki, device, shaderInterface, isImmutable, images, *descriptorSet);
	else
		DE_FATAL("Impossible");

	return descriptorSet;
}

void ImageSampleRenderInstance::writeSamplerDescriptorSet (const vk::DeviceInterface&		vki,
														   vk::VkDevice						device,
														   ShaderInputInterface				shaderInterface,
														   bool								isImmutable,
														   const ImageSampleInstanceImages&	images,
														   vk::VkDescriptorSet				descriptorSet)
{
	const vk::VkDescriptorInfo		imageInfo			= createDescriptorInfo(images.getImageViewA(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	const vk::VkDescriptorInfo		samplersInfos[2]	=
	{
		createDescriptorInfo(images.getSamplerA()),
		createDescriptorInfo(images.getSamplerB()),
	};

	vk::DescriptorSetUpdateBuilder	builder;

	// stand alone texture
	builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

	// samplers
	if (!isImmutable)
	{
		switch (shaderInterface)
		{
			case SHADER_INPUT_SINGLE_DESCRIPTOR:
				builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_SAMPLER, &samplersInfos[0]);
				break;

			case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
				builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_SAMPLER, &samplersInfos[0]);
				builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_SAMPLER, &samplersInfos[1]);
				break;

			case SHADER_INPUT_DESCRIPTOR_ARRAY:
				builder.writeArray(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_SAMPLER, 2u, samplersInfos);
				break;

			default:
				DE_FATAL("Impossible");
		}
	}

	builder.update(vki, device);
}

void ImageSampleRenderInstance::writeImageSamplerDescriptorSet (const vk::DeviceInterface&			vki,
																vk::VkDevice						device,
																ShaderInputInterface				shaderInterface,
																bool								isImmutable,
																const ImageSampleInstanceImages&	images,
																vk::VkDescriptorSet					descriptorSet)
{
	const vk::VkSampler				samplers[2]			=
	{
		(isImmutable) ? (0) : (images.getSamplerA()),
		(isImmutable) ? (0) : (images.getSamplerB()),
	};
	const vk::VkDescriptorInfo		imageSamplers[2]	=
	{
		createDescriptorInfo(samplers[0], images.getImageViewA(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		createDescriptorInfo(samplers[1], images.getImageViewB(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
	};

	vk::DescriptorSetUpdateBuilder	builder;

	// combined image samplers
	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageSamplers[0]);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageSamplers[0]);
			builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageSamplers[1]);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.writeArray(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2u, imageSamplers);
			break;

		default:
			DE_FATAL("Impossible");
	}

	builder.update(vki, device);
}

void ImageSampleRenderInstance::logTestPlan (void) const
{
	std::ostringstream msg;

	msg << "Rendering 2x2 grid.\n";

	if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
	{
		msg << "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
			<< " VK_DESCRIPTOR_TYPE_SAMPLER descriptor(s) and a single texture.\n";
	}
	else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		msg << "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
			<< " VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER descriptor(s).\n";
	}
	else
		DE_FATAL("Impossible");

	msg << "Image view type is " << vk::getImageViewTypeName(m_viewType) << "\n";

	if (m_baseMipLevel)
		msg << "Image view base mip level = " << m_baseMipLevel << "\n";
	if (m_baseArraySlice)
		msg << "Image view base array slice = " << m_baseArraySlice << "\n";

	if (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR)
		msg << "Sampler mode is LINEAR, with WRAP\n";
	else
		msg << "Sampler 0 mode is LINEAR, with WRAP\nSampler 1 mode is NEAREST with CLAMP\n";

	if (m_stageFlags == 0u)
	{
		msg << "Descriptors are not accessed in any shader stage.\n";
	}
	else
	{
		msg << "Color in each cell is fetched using the descriptor(s):\n";

		for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
		{
			msg << "Test sample " << resultNdx << ": sample at position " << m_images.getSamplePos(m_viewType, m_baseMipLevel, m_baseArraySlice, resultNdx);

			if (m_shaderInterface != SHADER_INPUT_SINGLE_DESCRIPTOR)
			{
				const int srcResourceNdx = (resultNdx % 2); // ABAB source

				if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
					msg << " using sampler " << srcResourceNdx;
				else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
					msg << " from combined image sampler " << srcResourceNdx;
				else
					DE_FATAL("Impossible");
			}
			msg << "\n";
		}

		msg << "Descriptors are accessed in {"
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_VERTEX_BIT) != 0)			? (" vertex")			: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_TESS_CONTROL_BIT) != 0)	? (" tess_control")		: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT) != 0)	? (" tess_evaluation")	: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_GEOMETRY_BIT) != 0)		? (" geometry")			: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_FRAGMENT_BIT) != 0)		? (" fragment")			: (""))
			<< " } stages.";
	}

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< msg.str()
		<< tcu::TestLog::EndMessage;
}

vk::VkPipelineLayout ImageSampleRenderInstance::getPipelineLayout (void) const
{
	return *m_pipelineLayout;
}

void ImageSampleRenderInstance::writeDrawCmdBuffer (vk::VkCmdBuffer cmd) const
{
	m_vki.cmdBindDescriptorSets(cmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, getPipelineLayout(), 0u, 1u, &m_descriptorSet.get(), 0u, DE_NULL);
	m_vki.cmdDraw(cmd, 6u * 4u, 1u, 0u, 0u); // render four quads (two separate triangles)
}

tcu::TestStatus ImageSampleRenderInstance::verifyResultImage (const tcu::ConstPixelBufferAccess& result) const
{
	const tcu::Vec4		green		(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4		yellow		(1.0f, 1.0f, 0.0f, 1.0f);
	const bool			doFetch		= (m_stageFlags != 0u); // no active stages? Then don't fetch
	const tcu::Vec4		sample0		= (!doFetch) ? (yellow)	: (m_images.fetchSampleValue(0));
	const tcu::Vec4		sample1		= (!doFetch) ? (green)	: (m_images.fetchSampleValue(1));
	const tcu::Vec4		sample2		= (!doFetch) ? (green)	: (m_images.fetchSampleValue(2));
	const tcu::Vec4		sample3		= (!doFetch) ? (yellow)	: (m_images.fetchSampleValue(3));
	const tcu::RGBA		threshold	= tcu::RGBA(8, 8, 8, 8); // source image is high-frequency so the threshold is quite large to tolerate sampling errors
	tcu::Surface		reference	(m_targetSize.x(), m_targetSize.y());

	drawQuadrantReferenceResult(reference.getAccess(), sample0, sample1, sample2, sample3);

	if (!bilinearCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", reference.getAccess(), result, threshold, tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Image verification failed");
	else
		return tcu::TestStatus::pass("Pass");
}

class ImageSampleComputeInstance : public vkt::TestInstance
{
public:
											ImageSampleComputeInstance		(vkt::Context&			context,
																			 vk::VkDescriptorType	descriptorType,
																			 ShaderInputInterface	shaderInterface,
																			 vk::VkImageViewType	viewType,
																			 deUint32				baseMipLevel,
																			 deUint32				baseArraySlice,
																			 bool					isImmutableSampler);

private:
	vk::Move<vk::VkDescriptorSetLayout>		createDescriptorSetLayout		(void) const;
	vk::Move<vk::VkDescriptorPool>			createDescriptorPool			(void) const;
	vk::Move<vk::VkDescriptorSet>			createDescriptorSet				(vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout) const;
	void									writeImageSamplerDescriptorSet	(vk::VkDescriptorSet descriptorSet) const;
	void									writeSamplerDescriptorSet		(vk::VkDescriptorSet descriptorSet) const;

	tcu::TestStatus							iterate							(void);
	void									logTestPlan						(void) const;
	tcu::TestStatus							testResourceAccess				(void);

	const vk::VkDescriptorType				m_descriptorType;
	const ShaderInputInterface				m_shaderInterface;
	const vk::VkImageViewType				m_viewType;
	const deUint32							m_baseMipLevel;
	const deUint32							m_baseArraySlice;
	const bool								m_isImmutableSampler;

	const vk::DeviceInterface&				m_vki;
	const vk::VkDevice						m_device;
	const vk::VkQueue						m_queue;
	const deUint32							m_queueFamilyIndex;
	vk::Allocator&							m_allocator;

	const ComputeInstanceResultBuffer		m_result;
	const ImageSampleInstanceImages			m_images;
};

ImageSampleComputeInstance::ImageSampleComputeInstance (Context&				context,
														vk::VkDescriptorType	descriptorType,
														ShaderInputInterface	shaderInterface,
														vk::VkImageViewType		viewType,
														deUint32				baseMipLevel,
														deUint32				baseArraySlice,
														bool					isImmutableSampler)
	: vkt::TestInstance		(context)
	, m_descriptorType		(descriptorType)
	, m_shaderInterface		(shaderInterface)
	, m_viewType			(viewType)
	, m_baseMipLevel		(baseMipLevel)
	, m_baseArraySlice		(baseArraySlice)
	, m_isImmutableSampler	(isImmutableSampler)
	, m_vki					(context.getDeviceInterface())
	, m_device				(context.getDevice())
	, m_queue				(context.getUniversalQueue())
	, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
	, m_allocator			(context.getDefaultAllocator())
	, m_result				(m_vki, m_device, m_allocator)
	, m_images				(m_vki, m_device, m_queueFamilyIndex, m_queue, m_allocator, m_descriptorType, m_shaderInterface, m_viewType, m_baseMipLevel, m_baseArraySlice, isImmutableSampler)
{
}

vk::Move<vk::VkDescriptorSetLayout> ImageSampleComputeInstance::createDescriptorSetLayout (void) const
{
	const vk::VkSampler				samplers[2] =
	{
		m_images.getSamplerA(),
		m_images.getSamplerB(),
	};

	vk::DescriptorSetLayoutBuilder	builder;

	// result buffer
	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);

	// with samplers, separate texture at binding 0
	if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
		builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);

	// (combined)samplers follow
	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.addSingleSamplerBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT, (m_images.isImmutable()) ? (&samplers[0]) : (DE_NULL));
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.addSingleSamplerBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT, (m_images.isImmutable()) ? (&samplers[0]) : (DE_NULL));
			builder.addSingleSamplerBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT, (m_images.isImmutable()) ? (&samplers[1]) : (DE_NULL));
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.addArraySamplerBinding(m_descriptorType, 2u, vk::VK_SHADER_STAGE_COMPUTE_BIT, (m_images.isImmutable()) ? (samplers) : (DE_NULL));
			break;

		default:
			DE_FATAL("Impossible");
	};

	return builder.build(m_vki, m_device);
}

vk::Move<vk::VkDescriptorPool> ImageSampleComputeInstance::createDescriptorPool (void) const
{
	vk::DescriptorPoolBuilder builder;

	builder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	builder.addType(m_descriptorType, getInterfaceNumResources(m_shaderInterface));

	if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
		builder.addType(vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	return builder.build(m_vki, m_device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);
}

vk::Move<vk::VkDescriptorSet> ImageSampleComputeInstance::createDescriptorSet (vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout) const
{
	vk::Move<vk::VkDescriptorSet> descriptorSet = allocDescriptorSet(m_vki, m_device, pool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);

	if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
		writeSamplerDescriptorSet(*descriptorSet);
	else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		writeImageSamplerDescriptorSet(*descriptorSet);
	else
		DE_FATAL("Impossible");

	return descriptorSet;
}

void ImageSampleComputeInstance::writeSamplerDescriptorSet (vk::VkDescriptorSet descriptorSet) const
{
	const vk::VkDescriptorInfo		resultInfo			= createDescriptorInfo(m_result.getBuffer(), 0u, (vk::VkDeviceSize)ComputeInstanceResultBuffer::DATA_SIZE);
	const vk::VkDescriptorInfo		imageInfo			= createDescriptorInfo(m_images.getImageViewA(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	const vk::VkDescriptorInfo		samplersInfos[2]	=
	{
		createDescriptorInfo(m_images.getSamplerA()),
		createDescriptorInfo(m_images.getSamplerB()),
	};

	vk::DescriptorSetUpdateBuilder	builder;

	// result
	builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo);

	// stand alone texture
	builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

	// samplers
	if (!m_isImmutableSampler)
	{
		switch (m_shaderInterface)
		{
			case SHADER_INPUT_SINGLE_DESCRIPTOR:
				builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_SAMPLER, &samplersInfos[0]);
				break;

			case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
				builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_SAMPLER, &samplersInfos[0]);
				builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(3u), vk::VK_DESCRIPTOR_TYPE_SAMPLER, &samplersInfos[1]);
				break;

			case SHADER_INPUT_DESCRIPTOR_ARRAY:
				builder.writeArray(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_SAMPLER, 2u, samplersInfos);
				break;

			default:
				DE_FATAL("Impossible");
		}
	}

	builder.update(m_vki, m_device);
}

void ImageSampleComputeInstance::writeImageSamplerDescriptorSet (vk::VkDescriptorSet descriptorSet) const
{
	const vk::VkDescriptorInfo		resultInfo			= createDescriptorInfo(m_result.getBuffer(), 0u, (vk::VkDeviceSize)ComputeInstanceResultBuffer::DATA_SIZE);
	const vk::VkSampler				samplers[2]			=
	{
		(m_isImmutableSampler) ? (0) : (m_images.getSamplerA()),
		(m_isImmutableSampler) ? (0) : (m_images.getSamplerB()),
	};
	const vk::VkDescriptorInfo		imageSamplers[2]	=
	{
		createDescriptorInfo(samplers[0], m_images.getImageViewA(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		createDescriptorInfo(samplers[1], m_images.getImageViewB(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
	};

	vk::DescriptorSetUpdateBuilder	builder;

	// result
	builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo);

	// combined image samplers
	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageSamplers[0]);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageSamplers[0]);
			builder.writeSingle(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageSamplers[1]);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.writeArray(descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2u, imageSamplers);
			break;

		default:
			DE_FATAL("Impossible");
	}

	builder.update(m_vki, m_device);
}

tcu::TestStatus ImageSampleComputeInstance::iterate (void)
{
	logTestPlan();
	return testResourceAccess();
}

void ImageSampleComputeInstance::logTestPlan (void) const
{
	std::ostringstream msg;

	msg << "Accessing resource in a compute program.\n";

	if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
	{
		msg << "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
			<< " VK_DESCRIPTOR_TYPE_SAMPLER descriptor(s) and a single texture.\n";
	}
	else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		msg << "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
			<< " VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER descriptor(s).\n";
	}
	else
		DE_FATAL("Impossible");

	msg << "Image view type is " << vk::getImageViewTypeName(m_viewType) << "\n";

	if (m_baseMipLevel)
		msg << "Image view base mip level = " << m_baseMipLevel << "\n";
	if (m_baseArraySlice)
		msg << "Image view base array slice = " << m_baseArraySlice << "\n";

	if (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR)
		msg << "Sampler mode is LINEAR, with WRAP\n";
	else
		msg << "Sampler 0 mode is LINEAR, with WRAP\nSampler 1 mode is NEAREST with CLAMP\n";

	for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
	{
		msg << "Test sample " << resultNdx << ": sample at position " << m_images.getSamplePos(m_viewType, m_baseMipLevel, m_baseArraySlice, resultNdx);

		if (m_shaderInterface != SHADER_INPUT_SINGLE_DESCRIPTOR)
		{
			const int srcResourceNdx = (resultNdx % 2); // ABAB source

			if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
				msg << " using sampler " << srcResourceNdx;
			else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				msg << " from combined image sampler " << srcResourceNdx;
			else
				DE_FATAL("Impossible");
		}
		msg << "\n";
	}

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< msg.str()
		<< tcu::TestLog::EndMessage;
}

tcu::TestStatus ImageSampleComputeInstance::testResourceAccess (void)
{
	const vk::Unique<vk::VkDescriptorSetLayout>		descriptorSetLayout	(createDescriptorSetLayout());
	const vk::Unique<vk::VkDescriptorPool>			descriptorPool		(createDescriptorPool());
	const vk::Unique<vk::VkDescriptorSet>			descriptorSet		(createDescriptorSet(*descriptorPool, *descriptorSetLayout));
	const ComputePipeline							pipeline			(m_vki, m_device, m_context.getBinaryCollection(), 1, &descriptorSetLayout.get());

	const vk::VkDescriptorSet						descriptorSets[]	= { *descriptorSet };
	const int										numDescriptorSets	= DE_LENGTH_OF_ARRAY(descriptorSets);
	const deUint32* const							dynamicOffsets		= DE_NULL;
	const int										numDynamicOffsets	= 0;
	const void*	const*								preBarriers			= DE_NULL;
	const int										numPreBarriers		= 0;
	const void* const								postBarriers[]		= { m_result.getResultReadBarrier() };
	const int										numPostBarriers		= DE_LENGTH_OF_ARRAY(postBarriers);

	const ComputeCommand							compute				(m_vki,
																		 m_device,
																		 pipeline.getPipeline(),
																		 pipeline.getPipelineLayout(),
																		 tcu::UVec3(4, 1, 1),
																		 numDescriptorSets,	descriptorSets,
																		 numDynamicOffsets,	dynamicOffsets,
																		 numPreBarriers,	preBarriers,
																		 numPostBarriers,	postBarriers);

	tcu::Vec4										results[4];
	bool											anyResultSet		= false;
	bool											allResultsOk		= true;

	compute.submitAndWait(m_queueFamilyIndex, m_queue);
	m_result.readResultContentsTo(&results);

	// verify
	for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
	{
		const tcu::Vec4	result				= results[resultNdx];
		const tcu::Vec4	reference			= m_images.fetchSampleValue(resultNdx);

		// source image is high-frequency so the threshold is quite large to tolerate sampling errors
		const tcu::Vec4	samplingThreshold	= tcu::Vec4(8.0f / 255.0f);

		if (result != tcu::Vec4(-1.0f))
			anyResultSet = true;

		if (tcu::boolAny(tcu::greaterThan(tcu::abs(result - reference), samplingThreshold)))
		{
			allResultsOk = false;

			m_context.getTestContext().getLog()
				<< tcu::TestLog::Message
				<< "Test sample " << resultNdx << ":\n"
				<< "\tSampling at " << m_images.getSamplePos(m_viewType, m_baseMipLevel, m_baseArraySlice, resultNdx) << "\n"
				<< "\tError expected " << reference << ", got " << result
				<< tcu::TestLog::EndMessage;
		}
	}

	// read back and verify
	if (allResultsOk)
		return tcu::TestStatus::pass("Pass");
	else if (anyResultSet)
		return tcu::TestStatus::fail("Invalid result values");
	else
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Result buffer was not written to."
			<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Result buffer was not written to");
	}
}

class ImageDescriptorCase : public QuadrantRendederCase
{
public:
	enum
	{
		FLAG_BASE_MIP	= (1u << 1u),
		FLAG_BASE_SLICE	= (1u << 2u),
	};
	// enum continues where resource flags ends
	DE_STATIC_ASSERT((deUint32)FLAG_BASE_MIP == (deUint32)RESOURCE_FLAG_LAST);

								ImageDescriptorCase			(tcu::TestContext&		testCtx,
															 const char*			name,
															 const char*			description,
															 bool					isPrimaryCmdBuf,
															 vk::VkDescriptorType	descriptorType,
															 vk::VkShaderStageFlags	exitingStages,
															 vk::VkShaderStageFlags	activeStages,
															 ShaderInputInterface	shaderInterface,
															 vk::VkImageViewType	viewType,
															 deUint32				flags);

private:
	std::string					genExtensionDeclarations	(vk::VkShaderStage stage) const;
	std::string					genResourceDeclarations		(vk::VkShaderStage stage, int numUsedBindings) const;
	std::string					genFetchCoordStr			(int fetchPosNdx) const;
	std::string					genSampleCoordStr			(int samplePosNdx) const;
	std::string					genResourceAccessSource		(vk::VkShaderStage stage) const;
	std::string					genNoAccessSource			(void) const;

	vkt::TestInstance*			createInstance				(vkt::Context& context) const;

private:
	const bool					m_isPrimaryCmdBuf;
	const vk::VkDescriptorType	m_descriptorType;
	const ShaderInputInterface	m_shaderInterface;
	const vk::VkImageViewType	m_viewType;
	const deUint32				m_baseMipLevel;
	const deUint32				m_baseArraySlice;
	const bool					m_isImmutableSampler;
};

ImageDescriptorCase::ImageDescriptorCase (tcu::TestContext&			testCtx,
										  const char*				name,
										  const char*				description,
										  bool						isPrimaryCmdBuf,
										  vk::VkDescriptorType		descriptorType,
										  vk::VkShaderStageFlags	exitingStages,
										  vk::VkShaderStageFlags	activeStages,
										  ShaderInputInterface		shaderInterface,
										  vk::VkImageViewType		viewType,
										  deUint32					flags)
	: QuadrantRendederCase	(testCtx, name, description,
							 // \note 1D textures are not supported in ES
							 (viewType == vk::VK_IMAGE_VIEW_TYPE_1D || viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY) ? glu::GLSL_VERSION_440 : glu::GLSL_VERSION_310_ES,
							 exitingStages, activeStages)
	, m_isPrimaryCmdBuf		(isPrimaryCmdBuf)
	, m_descriptorType		(descriptorType)
	, m_shaderInterface		(shaderInterface)
	, m_viewType			(viewType)
	, m_baseMipLevel		(((flags & FLAG_BASE_MIP) != 0) ? (1u) : (0u))
	, m_baseArraySlice		(((flags & FLAG_BASE_SLICE) != 0) ? (1u) : (0u))
	, m_isImmutableSampler	((flags & RESOURCE_FLAG_IMMUTABLE_SAMPLER) != 0)
{
}

std::string ImageDescriptorCase::genExtensionDeclarations (vk::VkShaderStage stage) const
{
	DE_UNREF(stage);

	if (m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
		return "#extension GL_OES_texture_cube_map_array : require\n";
	else
		return "";
}

std::string ImageDescriptorCase::genResourceDeclarations (vk::VkShaderStage stage, int numUsedBindings) const
{
	DE_UNREF(stage);

	// Vulkan-style resources are arrays implicitly, OpenGL-style are not
	const std::string	dimensionBase	= (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D || m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)		? ("1D")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D || m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY)		? ("2D")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_3D)															? ("3D")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE || m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)	? ("Cube")
										: (DE_NULL);
	const std::string	dimensionArray	= (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D || m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)		? ("1DArray")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D || m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY)		? ("2DArray")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_3D)															? ("3D")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE || m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)	? ("CubeArray")
										: (DE_NULL);
	const std::string	dimension		= isImageViewTypeArray(m_viewType) ? dimensionArray : dimensionBase;

	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
		{
			switch (m_descriptorType)
			{
				case vk::VK_DESCRIPTOR_TYPE_SAMPLER:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp texture" + dimension + " u_separateTexture;\n"
						   "layout(set = 0, binding = " + de::toString(numUsedBindings+1) + ") uniform highp sampler u_separateSampler;\n";
				case vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp sampler" + dimension + " u_combinedTextureSampler;\n";
				case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp texture" + dimensionBase + " u_separateTexture;\n";
				case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ", rgba8) readonly uniform highp image" + dimension + " u_image;\n";
				default:
					DE_FATAL("invalid descriptor");
					return "";
			}
		}

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			switch (m_descriptorType)
			{
				case vk::VK_DESCRIPTOR_TYPE_SAMPLER:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp texture" + dimension + " u_separateTexture;\n"
						   "layout(set = 0, binding = " + de::toString(numUsedBindings+1) + ") uniform highp sampler u_separateSamplerA;\n"
						   "layout(set = 0, binding = " + de::toString(numUsedBindings+2) + ") uniform highp sampler u_separateSamplerB;\n";
				case vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp sampler" + dimension + " u_combinedTextureSamplerA;\n"
						   "layout(set = 0, binding = " + de::toString(numUsedBindings+1) + ") uniform highp sampler" + dimension + " u_combinedTextureSamplerB;\n";
				case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp texture" + dimensionBase + " u_separateTextureA;\n"
						   "layout(set = 0, binding = " + de::toString(numUsedBindings+1) + ") uniform highp texture" + dimensionBase + " u_separateTextureB;\n";
				case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ", rgba8) readonly uniform highp image" + dimension + " u_imageA;\n"
						   "layout(set = 0, binding = " + de::toString(numUsedBindings+1) + ", rgba8) readonly uniform highp image" + dimension + " u_imageB;\n";
				default:
					DE_FATAL("invalid descriptor");
					return "";
			}

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			switch (m_descriptorType)
			{
				case vk::VK_DESCRIPTOR_TYPE_SAMPLER:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp texture" + dimension + " u_separateTexture;\n"
						   "layout(set = 0, binding = " + de::toString(numUsedBindings+1) + ") uniform highp sampler u_separateSampler[2];\n";
				case vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp sampler" + dimension + " u_combinedTextureSampler[2];\n";
				case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ") uniform highp texture" + dimensionBase + " u_separateTexture[2];\n";
				case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					return "layout(set = 0, binding = " + de::toString(numUsedBindings) + ", rgba8) readonly uniform highp image" + dimension + " u_image[2];\n";
				default:
					DE_FATAL("invalid descriptor");
					return "";
			}

		default:
			DE_FATAL("Impossible");
			return "";
	}
}

std::string ImageDescriptorCase::genFetchCoordStr (int fetchPosNdx) const
{
	DE_ASSERT(m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || m_descriptorType == vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	const tcu::IVec3 fetchPos = ImageFetchInstanceImages::getFetchPos(m_viewType, m_baseMipLevel, m_baseArraySlice, fetchPosNdx);

	if (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D)
	{
		return de::toString(fetchPos.x());
	}
	else if (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY || m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D)
	{
		std::ostringstream buf;
		buf << "ivec2(" << fetchPos.x() << ", " << fetchPos.y() << ")";
		return buf.str();
	}
	else
	{
		std::ostringstream buf;
		buf << "ivec3(" << fetchPos.x() << ", " << fetchPos.y() << ", " << fetchPos.z() << ")";
		return buf.str();
	}
}

std::string ImageDescriptorCase::genSampleCoordStr (int samplePosNdx) const
{
	DE_ASSERT(m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER || m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	const tcu::Vec4 fetchPos = ImageSampleInstanceImages::getSamplePos(m_viewType, m_baseMipLevel, m_baseArraySlice, samplePosNdx);

	if (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D)
	{
		std::ostringstream buf;
		buf << "float(" << fetchPos.x() << ")";
		return buf.str();
	}
	else if (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY || m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D)
	{
		std::ostringstream buf;
		buf << "vec2(float(" << fetchPos.x() << "), float(" << fetchPos.y() << "))";
		return buf.str();
	}
	else if (m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
	{
		std::ostringstream buf;
		buf << "vec4(float(" << fetchPos.x() << "), float(" << fetchPos.y() << "), float(" << fetchPos.z() << "), float(" << fetchPos.w() << "))";
		return buf.str();
	}
	else
	{
		std::ostringstream buf;
		buf << "vec3(float(" << fetchPos.x() << "), float(" << fetchPos.y() << "), float(" << fetchPos.z() << "))";
		return buf.str();
	}
}

std::string ImageDescriptorCase::genResourceAccessSource (vk::VkShaderStage stage) const
{
	DE_UNREF(stage);

	const char* const	dimension		= (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D)			? ("1D")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY)	? ("1DArray")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D)			? ("2D")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY)	? ("2DArray")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_3D)			? ("3D")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE)		? ("Cube")
										: (m_viewType == vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)	? ("CubeArray")
										: (DE_NULL);
	const char* const	accessPostfixA	= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR)		? ("")
										: (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS)	? ("A")
										: (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY)		? ("[0]")
										: (DE_NULL);
	const char* const	accessPostfixB	= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR)		? ("")
										: (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS)	? ("B")
										: (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY)		? ("[1]")
										: (DE_NULL);

	switch (m_descriptorType)
	{
		case vk::VK_DESCRIPTOR_TYPE_SAMPLER:
		case vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		{
			const std::string	coodStr[4]	=
			{
				genSampleCoordStr(0),
				genSampleCoordStr(1),
				genSampleCoordStr(2),
				genSampleCoordStr(3),
			};
			std::ostringstream	buf;

			if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_SAMPLER)
			{
				buf << "	if (quadrant_id == 0)\n"
					<< "		result_color = textureLod(sampler" << dimension << "(u_separateTexture, u_separateSampler" << accessPostfixA << "), " << coodStr[0] << ", 0.0);\n"
					<< "	else if (quadrant_id == 1)\n"
					<< "		result_color = textureLod(sampler" << dimension << "(u_separateTexture, u_separateSampler" << accessPostfixB << "), " << coodStr[1] << ", 0.0);\n"
					<< "	else if (quadrant_id == 2)\n"
					<< "		result_color = textureLod(sampler" << dimension << "(u_separateTexture, u_separateSampler" << accessPostfixA << "), " << coodStr[2] << ", 0.0);\n"
					<< "	else\n"
					<< "		result_color = textureLod(sampler" << dimension << "(u_separateTexture, u_separateSampler" << accessPostfixB << "), " << coodStr[3] << ", 0.0);\n";
			}
			else
			{
				buf << "	if (quadrant_id == 0)\n"
					<< "		result_color = textureLod(u_combinedTextureSampler" << accessPostfixA << ", " << coodStr[0] << ", 0.0);\n"
					<< "	else if (quadrant_id == 1)\n"
					<< "		result_color = textureLod(u_combinedTextureSampler" << accessPostfixB << ", " << coodStr[1] << ", 0.0);\n"
					<< "	else if (quadrant_id == 2)\n"
					<< "		result_color = textureLod(u_combinedTextureSampler" << accessPostfixA << ", " << coodStr[2] << ", 0.0);\n"
					<< "	else\n"
					<< "		result_color = textureLod(u_combinedTextureSampler" << accessPostfixB << ", " << coodStr[3] << ", 0.0);\n";
			}

			return buf.str();
		}

		case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		{
			const std::string	coodStr[4]	=
			{
				genFetchCoordStr(0),
				genFetchCoordStr(1),
				genFetchCoordStr(2),
				genFetchCoordStr(3),
			};
			std::ostringstream	buf;

			buf << "	if (quadrant_id == 0)\n"
				<< "		result_color = imageLoad(u_image" << accessPostfixA << ", " << coodStr[0] << ");\n"
				<< "	else if (quadrant_id == 1)\n"
				<< "		result_color = imageLoad(u_image" << accessPostfixB << ", " << coodStr[1] << ");\n"
				<< "	else if (quadrant_id == 2)\n"
				<< "		result_color = imageLoad(u_image" << accessPostfixA << ", " << coodStr[2] << ");\n"
				<< "	else\n"
				<< "		result_color = imageLoad(u_image" << accessPostfixB << ", " << coodStr[3] << ");\n";

			return buf.str();
		}

		default:
			DE_FATAL("invalid descriptor");
			return "";
	}
}

std::string ImageDescriptorCase::genNoAccessSource (void) const
{
	return "	if (quadrant_id == 1 || quadrant_id == 2)\n"
			"		result_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
			"	else\n"
			"		result_color = vec4(1.0, 1.0, 0.0, 1.0);\n";
}

vkt::TestInstance* ImageDescriptorCase::createInstance (vkt::Context& context) const
{
	switch (m_descriptorType)
	{
		case vk::VK_DESCRIPTOR_TYPE_SAMPLER:
		case vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			if (m_exitingStages == vk::VK_SHADER_STAGE_COMPUTE_BIT)
			{
				DE_ASSERT(m_isPrimaryCmdBuf);
				return new ImageSampleComputeInstance(context, m_descriptorType, m_shaderInterface, m_viewType, m_baseMipLevel, m_baseArraySlice, m_isImmutableSampler);
			}
			else
				return new ImageSampleRenderInstance(context, m_isPrimaryCmdBuf, m_descriptorType, m_activeStages, m_shaderInterface, m_viewType, m_baseMipLevel, m_baseArraySlice, m_isImmutableSampler);

		case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			if (m_exitingStages == vk::VK_SHADER_STAGE_COMPUTE_BIT)
			{
				DE_ASSERT(m_isPrimaryCmdBuf);
				return new ImageFetchComputeInstance(context, m_descriptorType, m_shaderInterface, m_viewType, m_baseMipLevel, m_baseArraySlice);
			}
			else
				return new ImageFetchRenderInstance(context, m_isPrimaryCmdBuf, m_descriptorType, m_activeStages, m_shaderInterface, m_viewType, m_baseMipLevel, m_baseArraySlice);

		default:
			DE_FATAL("Impossible");
			return DE_NULL;
	}
}

class TexelBufferInstanceBuffers
{
public:
										TexelBufferInstanceBuffers	(const vk::DeviceInterface&		vki,
																	 vk::VkDevice					device,
																	 vk::Allocator&					allocator,
																	 vk::VkDescriptorType			descriptorType,
																	 int							numTexelBuffers,
																	 bool							hasViewOffset);

private:
	static vk::Move<vk::VkBuffer>		createBuffer				(const vk::DeviceInterface&		vki,
																	 vk::VkDevice					device,
																	 vk::Allocator&					allocator,
																	 vk::VkDescriptorType			descriptorType,
																	 de::MovePtr<vk::Allocation>	*outAllocation);

	static vk::Move<vk::VkBufferView>	createBufferView			(const vk::DeviceInterface&		vki,
																	 vk::VkDevice					device,
																	 const tcu::TextureFormat&		textureFormat,
																	 deUint32						offset,
																	 vk::VkBuffer					buffer);

	static vk::VkBufferMemoryBarrier	createBarrier				(vk::VkDescriptorType descriptorType, vk::VkBuffer buffer);

	void								populateSourceBuffer		(const tcu::PixelBufferAccess& access);
	void								uploadData					(const vk::DeviceInterface& vki, vk::VkDevice device, const vk::Allocation& memory, const de::ArrayBuffer<deUint8>& data);

public:
	static int							getFetchPos					(int fetchPosNdx);
	tcu::Vec4							fetchTexelValue				(int fetchPosNdx) const;

	inline int							getNumTexelBuffers			(void) const { return m_numTexelBuffers;	}
	const tcu::TextureFormat&			getTextureFormat			(void) const { return m_imageFormat;		}
	inline vk::VkBufferView				getBufferViewA				(void) const { return *m_bufferViewA;		}
	inline vk::VkBufferView				getBufferViewB				(void) const { return *m_bufferViewB;		}
	inline const void*					getBufferInitBarrierA		(void) const { return &m_bufferBarrierA;	}
	inline const void*					getBufferInitBarrierB		(void) const { return &m_bufferBarrierB;	}

private:
	enum
	{
		BUFFER_SIZE			= 512,
		VIEW_OFFSET_VALUE	= 256,
		VIEW_DATA_SIZE		= 256,	//!< size in bytes
		VIEW_WIDTH			= 64,	//!< size in pixels
	};
	enum
	{
		// some arbitrary points
		SAMPLE_POINT_0 = 6,
		SAMPLE_POINT_1 = 51,
		SAMPLE_POINT_2 = 42,
		SAMPLE_POINT_3 = 25,
	};

	const deUint32						m_numTexelBuffers;
	const tcu::TextureFormat			m_imageFormat;
	const deUint32						m_viewOffset;

	de::ArrayBuffer<deUint8>			m_sourceBufferA;
	de::ArrayBuffer<deUint8>			m_sourceBufferB;
	const tcu::ConstPixelBufferAccess	m_sourceViewA;
	const tcu::ConstPixelBufferAccess	m_sourceViewB;

	de::MovePtr<vk::Allocation>			m_bufferMemoryA;
	de::MovePtr<vk::Allocation>			m_bufferMemoryB;
	const vk::Unique<vk::VkBuffer>		m_bufferA;
	const vk::Unique<vk::VkBuffer>		m_bufferB;
	const vk::Unique<vk::VkBufferView>	m_bufferViewA;
	const vk::Unique<vk::VkBufferView>	m_bufferViewB;
	const vk::VkBufferMemoryBarrier		m_bufferBarrierA;
	const vk::VkBufferMemoryBarrier		m_bufferBarrierB;
};

TexelBufferInstanceBuffers::TexelBufferInstanceBuffers (const vk::DeviceInterface&		vki,
														vk::VkDevice					device,
														vk::Allocator&					allocator,
														vk::VkDescriptorType			descriptorType,
														int								numTexelBuffers,
														bool							hasViewOffset)
	: m_numTexelBuffers	(numTexelBuffers)
	, m_imageFormat		(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)
	, m_viewOffset		((hasViewOffset) ? ((deUint32)VIEW_OFFSET_VALUE) : (0u))
	, m_sourceBufferA	(BUFFER_SIZE)
	, m_sourceBufferB	((numTexelBuffers == 1)
							? (0u)
							: ((size_t)BUFFER_SIZE))
	, m_sourceViewA		(m_imageFormat, tcu::IVec3(VIEW_WIDTH, 1, 1), m_sourceBufferA.getElementPtr(m_viewOffset))
	, m_sourceViewB		(m_imageFormat, tcu::IVec3(VIEW_WIDTH, 1, 1), m_sourceBufferB.getElementPtr(m_viewOffset))
	, m_bufferMemoryA	(DE_NULL)
	, m_bufferMemoryB	(DE_NULL)
	, m_bufferA			(createBuffer(vki, device, allocator, descriptorType, &m_bufferMemoryA))
	, m_bufferB			((numTexelBuffers == 1)
							? vk::Move<vk::VkBuffer>()
							: createBuffer(vki, device, allocator, descriptorType, &m_bufferMemoryB))
	, m_bufferViewA		(createBufferView(vki, device, m_imageFormat, m_viewOffset, *m_bufferA))
	, m_bufferViewB		((numTexelBuffers == 1)
							? vk::Move<vk::VkBufferView>()
							: createBufferView(vki, device, m_imageFormat, m_viewOffset, *m_bufferB))
	, m_bufferBarrierA	(createBarrier(descriptorType, *m_bufferA))
	, m_bufferBarrierB	(createBarrier(descriptorType, *m_bufferB))
{
	DE_ASSERT(numTexelBuffers == 1 || numTexelBuffers == 2);
	DE_ASSERT(VIEW_WIDTH * m_imageFormat.getPixelSize() == VIEW_DATA_SIZE);
	DE_ASSERT(BUFFER_SIZE % m_imageFormat.getPixelSize() == 0);

	// specify and upload

	populateSourceBuffer(tcu::PixelBufferAccess(m_imageFormat, tcu::IVec3(BUFFER_SIZE / m_imageFormat.getPixelSize(), 1, 1), m_sourceBufferA.getPtr()));
	uploadData(vki, device, *m_bufferMemoryA, m_sourceBufferA);

	if (numTexelBuffers == 2)
	{
		populateSourceBuffer(tcu::PixelBufferAccess(m_imageFormat, tcu::IVec3(BUFFER_SIZE / m_imageFormat.getPixelSize(), 1, 1), m_sourceBufferB.getPtr()));
		uploadData(vki, device, *m_bufferMemoryB, m_sourceBufferB);
	}
}

vk::Move<vk::VkBuffer> TexelBufferInstanceBuffers::createBuffer (const vk::DeviceInterface&		vki,
																 vk::VkDevice					device,
																 vk::Allocator&					allocator,
																 vk::VkDescriptorType			descriptorType,
																 de::MovePtr<vk::Allocation>	*outAllocation)
{
	const vk::VkBufferUsageFlags	usage		= (isUniformDescriptorType(descriptorType)) ? (vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) : (vk::VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
	const vk::VkBufferCreateInfo	createInfo	=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		(vk::VkDeviceSize)BUFFER_SIZE,		// size
		usage,								// usage
		0u,									// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,		// sharingMode
		0u,									// queueFamilyCount
		DE_NULL,							// pQueueFamilyIndices
	};
	vk::Move<vk::VkBuffer>			buffer		(vk::createBuffer(vki, device, &createInfo));
	de::MovePtr<vk::Allocation>		allocation	(allocateAndBindObjectMemory(vki, device, allocator, *buffer, vk::MemoryRequirement::HostVisible));

	*outAllocation = allocation;
	return buffer;
}

vk::Move<vk::VkBufferView> TexelBufferInstanceBuffers::createBufferView (const vk::DeviceInterface&		vki,
																		 vk::VkDevice					device,
																		 const tcu::TextureFormat&		textureFormat,
																		 deUint32						offset,
																		 vk::VkBuffer					buffer)
{
	const vk::VkBufferViewCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
		DE_NULL,
		buffer,									// buffer
		mapToVkTextureFormat(textureFormat),	// format
		(vk::VkDeviceSize)offset,				// offset
		(vk::VkDeviceSize)VIEW_DATA_SIZE		// range
	};
	return vk::createBufferView(vki, device, &createInfo);
}

vk::VkBufferMemoryBarrier TexelBufferInstanceBuffers::createBarrier (vk::VkDescriptorType descriptorType, vk::VkBuffer buffer)
{
	const vk::VkMemoryInputFlags	inputBit	= (isUniformDescriptorType(descriptorType)) ? (vk::VK_MEMORY_INPUT_UNIFORM_READ_BIT) : (vk::VK_MEMORY_INPUT_SHADER_READ_BIT);
	const vk::VkBufferMemoryBarrier	barrier		=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_HOST_WRITE_BIT,	// outputMask
		inputBit,								// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,			// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,			// destQueueFamilyIndex
		buffer	,								// buffer
		0u,										// offset
		(vk::VkDeviceSize)BUFFER_SIZE			// size
	};
	return barrier;
}

void TexelBufferInstanceBuffers::populateSourceBuffer (const tcu::PixelBufferAccess& access)
{
	DE_ASSERT(access.getHeight() == 1);
	DE_ASSERT(access.getDepth() == 1);

	const deInt32 width = access.getWidth();

	for (int x = 0; x < width; ++x)
	{
		const int			red		= 255 * x / width;												//!< gradient from 0 -> max (detects large offset errors)
		const int			green	= ((x % 2 == 0) ? (127) : (0)) + ((x % 4 < 3) ? (128) : (0));	//!< 3-level M pattern (detects small offset errors)
		const int			blue	= 16 * (x % 16);												//!< 16-long triangle wave

		DE_ASSERT(de::inRange(red, 0, 255));
		DE_ASSERT(de::inRange(green, 0, 255));
		DE_ASSERT(de::inRange(blue, 0, 255));

		access.setPixel(tcu::IVec4(red, green, blue, 255), x, 0, 0);
	}
}

void TexelBufferInstanceBuffers::uploadData (const vk::DeviceInterface& vki, vk::VkDevice device, const vk::Allocation& memory, const de::ArrayBuffer<deUint8>& data)
{
	deMemcpy(memory.getHostPtr(), data.getPtr(), data.size());
	flushMappedMemoryRange(vki, device, memory.getMemory(), memory.getOffset(), data.size());
}

int TexelBufferInstanceBuffers::getFetchPos (int fetchPosNdx)
{
	static const int fetchPositions[4] =
	{
		SAMPLE_POINT_0,
		SAMPLE_POINT_1,
		SAMPLE_POINT_2,
		SAMPLE_POINT_3,
	};
	return de::getSizedArrayElement<4>(fetchPositions, fetchPosNdx);
}

tcu::Vec4 TexelBufferInstanceBuffers::fetchTexelValue (int fetchPosNdx) const
{
	// source order is ABAB
	const tcu::ConstPixelBufferAccess&	texelSrcA	= m_sourceViewA;
	const tcu::ConstPixelBufferAccess&	texelSrcB	= (m_numTexelBuffers == 1) ? (m_sourceViewA) : (m_sourceViewB);
	const tcu::ConstPixelBufferAccess&	texelSrc	= ((fetchPosNdx % 2) == 0) ? (texelSrcA) : (texelSrcB);

	return texelSrc.getPixel(getFetchPos(fetchPosNdx), 0, 0);
}

class TexelBufferRenderInstance : public SingleCmdRenderInstance
{
public:
													TexelBufferRenderInstance	(vkt::Context&			context,
																				 bool					isPrimaryCmdBuf,
																				 vk::VkDescriptorType	descriptorType,
																				 vk::VkShaderStageFlags	stageFlags,
																				 ShaderInputInterface	shaderInterface,
																				 bool					nonzeroViewOffset);

private:
	static vk::Move<vk::VkDescriptorSetLayout>		createDescriptorSetLayout	(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface,
																				 vk::VkShaderStageFlags		stageFlags);

	static vk::Move<vk::VkPipelineLayout>			createPipelineLayout		(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorSetLayout	descriptorSetLayout);

	static vk::Move<vk::VkDescriptorPool>			createDescriptorPool		(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface);

	static vk::Move<vk::VkDescriptorSet>			createDescriptorSet			(const vk::DeviceInterface&	vki,
																				 vk::VkDevice				device,
																				 vk::VkDescriptorType		descriptorType,
																				 ShaderInputInterface		shaderInterface,
																				 vk::VkDescriptorSetLayout	layout,
																				 vk::VkDescriptorPool		pool,
																				 vk::VkBufferView			viewA,
																				 vk::VkBufferView			viewB);

	void											logTestPlan					(void) const;
	vk::VkPipelineLayout							getPipelineLayout			(void) const;
	void											writeDrawCmdBuffer			(vk::VkCmdBuffer cmd) const;
	tcu::TestStatus									verifyResultImage			(const tcu::ConstPixelBufferAccess& result) const;

	enum
	{
		RENDER_SIZE = 128,
	};

	const vk::VkDescriptorType						m_descriptorType;
	const vk::VkShaderStageFlags					m_stageFlags;
	const ShaderInputInterface						m_shaderInterface;
	const bool										m_nonzeroViewOffset;

	const vk::Unique<vk::VkDescriptorSetLayout>		m_descriptorSetLayout;
	const vk::Unique<vk::VkPipelineLayout>			m_pipelineLayout;
	const TexelBufferInstanceBuffers				m_texelBuffers;
	const vk::Unique<vk::VkDescriptorPool>			m_descriptorPool;
	const vk::Unique<vk::VkDescriptorSet>			m_descriptorSet;
};

TexelBufferRenderInstance::TexelBufferRenderInstance (vkt::Context&				context,
													  bool						isPrimaryCmdBuf,
													  vk::VkDescriptorType		descriptorType,
													  vk::VkShaderStageFlags	stageFlags,
													  ShaderInputInterface		shaderInterface,
													  bool						nonzeroViewOffset)
	: SingleCmdRenderInstance	(context, isPrimaryCmdBuf, tcu::UVec2(RENDER_SIZE, RENDER_SIZE))
	, m_descriptorType			(descriptorType)
	, m_stageFlags				(stageFlags)
	, m_shaderInterface			(shaderInterface)
	, m_nonzeroViewOffset		(nonzeroViewOffset)
	, m_descriptorSetLayout		(createDescriptorSetLayout(m_vki, m_device, m_descriptorType, m_shaderInterface, m_stageFlags))
	, m_pipelineLayout			(createPipelineLayout(m_vki, m_device, *m_descriptorSetLayout))
	, m_texelBuffers			(m_vki, m_device, m_allocator, m_descriptorType, getInterfaceNumResources(m_shaderInterface), m_nonzeroViewOffset)
	, m_descriptorPool			(createDescriptorPool(m_vki, m_device, m_descriptorType, m_shaderInterface))
	, m_descriptorSet			(createDescriptorSet(m_vki, m_device, m_descriptorType, m_shaderInterface, *m_descriptorSetLayout, *m_descriptorPool, m_texelBuffers.getBufferViewA(), m_texelBuffers.getBufferViewB()))
{
}

vk::Move<vk::VkDescriptorSetLayout> TexelBufferRenderInstance::createDescriptorSetLayout (const vk::DeviceInterface&	vki,
																						  vk::VkDevice					device,
																						  vk::VkDescriptorType			descriptorType,
																						  ShaderInputInterface			shaderInterface,
																						  vk::VkShaderStageFlags		stageFlags)
{
	vk::DescriptorSetLayoutBuilder builder;

	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.addSingleBinding(descriptorType, stageFlags);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.addSingleBinding(descriptorType, stageFlags);
			builder.addSingleBinding(descriptorType, stageFlags);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.addArrayBinding(descriptorType, 2u, stageFlags);
			break;

		default:
			DE_FATAL("Impossible");
	}

	return builder.build(vki, device);
}

vk::Move<vk::VkPipelineLayout> TexelBufferRenderInstance::createPipelineLayout (const vk::DeviceInterface&	vki,
																				vk::VkDevice				device,
																				vk::VkDescriptorSetLayout	descriptorSetLayout)
{
	const vk::VkPipelineLayoutCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		1,						// descriptorSetCount
		&descriptorSetLayout,	// pSetLayouts
		0u,						// pushConstantRangeCount
		DE_NULL,				// pPushConstantRanges
	};
	return vk::createPipelineLayout(vki, device, &createInfo);
}

vk::Move<vk::VkDescriptorPool> TexelBufferRenderInstance::createDescriptorPool (const vk::DeviceInterface&	vki,
																				vk::VkDevice					device,
																				vk::VkDescriptorType			descriptorType,
																				ShaderInputInterface			shaderInterface)
{
	return vk::DescriptorPoolBuilder()
		.addType(descriptorType, getInterfaceNumResources(shaderInterface))
		.build(vki, device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);
}

vk::Move<vk::VkDescriptorSet> TexelBufferRenderInstance::createDescriptorSet (const vk::DeviceInterface&	vki,
																			  vk::VkDevice					device,
																			  vk::VkDescriptorType			descriptorType,
																			  ShaderInputInterface			shaderInterface,
																			  vk::VkDescriptorSetLayout		layout,
																			  vk::VkDescriptorPool			pool,
																			  vk::VkBufferView				viewA,
																			  vk::VkBufferView				viewB)
{
	const vk::VkDescriptorInfo		texelBufferInfos[2]	=
	{
		createDescriptorInfo(viewA),
		createDescriptorInfo(viewB),
	};

	vk::Move<vk::VkDescriptorSet>	descriptorSet		= allocDescriptorSet(vki, device, pool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);
	vk::DescriptorSetUpdateBuilder	builder;

	switch (shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &texelBufferInfos[0]);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &texelBufferInfos[0]);
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), descriptorType, &texelBufferInfos[1]);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.writeArray(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, 2u, texelBufferInfos);
			break;

		default:
			DE_FATAL("Impossible");
	}

	builder.update(vki, device);
	return descriptorSet;
}

void TexelBufferRenderInstance::logTestPlan (void) const
{
	std::ostringstream msg;

	msg << "Rendering 2x2 grid.\n"
		<< "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
		<< " descriptor(s) of type " << vk::getDescriptorTypeName(m_descriptorType) << "\n"
		<< "Buffer view is created with a " << ((m_nonzeroViewOffset) ? ("non-zero") : ("zero")) << " offset.\n"
		<< "Buffer format is " << vk::getFormatName(mapToVkTextureFormat(m_texelBuffers.getTextureFormat())) << ".\n";

	if (m_stageFlags == 0u)
	{
		msg << "Descriptors are not accessed in any shader stage.\n";
	}
	else
	{
		msg << "Color in each cell is fetched using the descriptor(s):\n";

		for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
		{
			msg << "Test sample " << resultNdx << ": fetch at position " << m_texelBuffers.getFetchPos(resultNdx);

			if (m_shaderInterface != SHADER_INPUT_SINGLE_DESCRIPTOR)
			{
				const int srcResourceNdx = (resultNdx % 2); // ABAB source
				msg << " from texelBuffer " << srcResourceNdx;
			}

			msg << "\n";
		}

		msg << "Descriptors are accessed in {"
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_VERTEX_BIT) != 0)			? (" vertex")			: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_TESS_CONTROL_BIT) != 0)	? (" tess_control")		: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT) != 0)	? (" tess_evaluation")	: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_GEOMETRY_BIT) != 0)		? (" geometry")			: (""))
			<< (((m_stageFlags & vk::VK_SHADER_STAGE_FRAGMENT_BIT) != 0)		? (" fragment")			: (""))
			<< " } stages.";
	}

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< msg.str()
		<< tcu::TestLog::EndMessage;
}

vk::VkPipelineLayout TexelBufferRenderInstance::getPipelineLayout (void) const
{
	return *m_pipelineLayout;
}

void TexelBufferRenderInstance::writeDrawCmdBuffer (vk::VkCmdBuffer cmd) const
{
	m_vki.cmdBindDescriptorSets(cmd, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, getPipelineLayout(), 0, 1, &m_descriptorSet.get(), 0, DE_NULL);
	m_vki.cmdDraw(cmd, 6 * 4, 1, 0, 0); // render four quads (two separate triangles)
}

tcu::TestStatus TexelBufferRenderInstance::verifyResultImage (const tcu::ConstPixelBufferAccess& result) const
{
	const tcu::Vec4		green		(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4		yellow		(1.0f, 1.0f, 0.0f, 1.0f);
	const bool			doFetch		= (m_stageFlags != 0u); // no active stages? Then don't fetch
	const tcu::Vec4		sample0		= (!doFetch) ? (yellow)	: (m_texelBuffers.fetchTexelValue(0));
	const tcu::Vec4		sample1		= (!doFetch) ? (green)	: (m_texelBuffers.fetchTexelValue(1));
	const tcu::Vec4		sample2		= (!doFetch) ? (green)	: (m_texelBuffers.fetchTexelValue(2));
	const tcu::Vec4		sample3		= (!doFetch) ? (yellow)	: (m_texelBuffers.fetchTexelValue(3));
	tcu::Surface		reference	(m_targetSize.x(), m_targetSize.y());

	drawQuadrantReferenceResult(reference.getAccess(), sample0, sample1, sample2, sample3);

	if (!bilinearCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", reference.getAccess(), result, tcu::RGBA(1, 1, 1, 1), tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Image verification failed");
	else
		return tcu::TestStatus::pass("Pass");
}

class TexelBufferComputeInstance : public vkt::TestInstance
{
public:
											TexelBufferComputeInstance	(vkt::Context&			context,
																		 vk::VkDescriptorType	descriptorType,
																		 ShaderInputInterface	shaderInterface,
																		 bool					nonzeroViewOffset);

private:
	vk::Move<vk::VkDescriptorSetLayout>		createDescriptorSetLayout	(void) const;
	vk::Move<vk::VkDescriptorPool>			createDescriptorPool		(void) const;
	vk::Move<vk::VkDescriptorSet>			createDescriptorSet			(vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout) const;

	tcu::TestStatus							iterate						(void);
	void									logTestPlan					(void) const;
	tcu::TestStatus							testResourceAccess			(void);

	const vk::VkDescriptorType				m_descriptorType;
	const ShaderInputInterface				m_shaderInterface;
	const bool								m_nonzeroViewOffset;

	const vk::DeviceInterface&				m_vki;
	const vk::VkDevice						m_device;
	const vk::VkQueue						m_queue;
	const deUint32							m_queueFamilyIndex;
	vk::Allocator&							m_allocator;

	const ComputeInstanceResultBuffer		m_result;
	const TexelBufferInstanceBuffers		m_texelBuffers;
};

TexelBufferComputeInstance::TexelBufferComputeInstance (Context&				context,
														vk::VkDescriptorType	descriptorType,
														ShaderInputInterface	shaderInterface,
														bool					nonzeroViewOffset)
	: vkt::TestInstance		(context)
	, m_descriptorType		(descriptorType)
	, m_shaderInterface		(shaderInterface)
	, m_nonzeroViewOffset	(nonzeroViewOffset)
	, m_vki					(context.getDeviceInterface())
	, m_device				(context.getDevice())
	, m_queue				(context.getUniversalQueue())
	, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
	, m_allocator			(context.getDefaultAllocator())
	, m_result				(m_vki, m_device, m_allocator)
	, m_texelBuffers		(m_vki, m_device, m_allocator, m_descriptorType, getInterfaceNumResources(m_shaderInterface), m_nonzeroViewOffset)
{
}

vk::Move<vk::VkDescriptorSetLayout> TexelBufferComputeInstance::createDescriptorSetLayout (void) const
{
	vk::DescriptorSetLayoutBuilder builder;

	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);

	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			builder.addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.addArrayBinding(m_descriptorType, 2u, vk::VK_SHADER_STAGE_COMPUTE_BIT);
			break;

		default:
			DE_FATAL("Impossible");
	};

	return builder.build(m_vki, m_device);
}

vk::Move<vk::VkDescriptorPool> TexelBufferComputeInstance::createDescriptorPool (void) const
{
	return vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(m_descriptorType, getInterfaceNumResources(m_shaderInterface))
		.build(m_vki, m_device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);
}

vk::Move<vk::VkDescriptorSet> TexelBufferComputeInstance::createDescriptorSet (vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout) const
{
	const vk::VkDescriptorInfo		resultInfo			= createDescriptorInfo(m_result.getBuffer(), 0u, (vk::VkDeviceSize)ComputeInstanceResultBuffer::DATA_SIZE);
	const vk::VkDescriptorInfo		texelBufferInfos[2]	=
	{
		createDescriptorInfo(m_texelBuffers.getBufferViewA()),
		createDescriptorInfo(m_texelBuffers.getBufferViewB()),
	};

	vk::Move<vk::VkDescriptorSet>	descriptorSet		= allocDescriptorSet(m_vki, m_device, pool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);
	vk::DescriptorSetUpdateBuilder	builder;

	// result
	builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo);

	// texel buffers
	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, &texelBufferInfos[0]);
			break;

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, &texelBufferInfos[0]);
			builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), m_descriptorType, &texelBufferInfos[1]);
			break;

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			builder.writeArray(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), m_descriptorType, 2u, texelBufferInfos);
			break;

		default:
			DE_FATAL("Impossible");
	}

	builder.update(m_vki, m_device);
	return descriptorSet;
}

tcu::TestStatus TexelBufferComputeInstance::iterate (void)
{
	logTestPlan();
	return testResourceAccess();
}

void TexelBufferComputeInstance::logTestPlan (void) const
{
	std::ostringstream msg;

	msg << "Fetching 4 values from image in compute shader.\n"
		<< "Single descriptor set. Descriptor set contains "
			<< ((m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR) ? "single" :
			    (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS) ? "two" :
			    (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY) ? "an array (size 2) of" :
			    (const char*)DE_NULL)
		<< " descriptor(s) of type " << vk::getDescriptorTypeName(m_descriptorType) << "\n"
		<< "Buffer view is created with a " << ((m_nonzeroViewOffset) ? ("non-zero") : ("zero")) << " offset.\n"
		<< "Buffer format is " << vk::getFormatName(mapToVkTextureFormat(m_texelBuffers.getTextureFormat())) << ".\n";

	for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
	{
		msg << "Test sample " << resultNdx << ": fetch at position " << m_texelBuffers.getFetchPos(resultNdx);

		if (m_shaderInterface != SHADER_INPUT_SINGLE_DESCRIPTOR)
		{
			const int srcResourceNdx = (resultNdx % 2); // ABAB source
			msg << " from texelBuffer " << srcResourceNdx;
		}

		msg << "\n";
	}

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< msg.str()
		<< tcu::TestLog::EndMessage;
}

tcu::TestStatus TexelBufferComputeInstance::testResourceAccess (void)
{
	const vk::Unique<vk::VkDescriptorSetLayout>		descriptorSetLayout	(createDescriptorSetLayout());
	const vk::Unique<vk::VkDescriptorPool>			descriptorPool		(createDescriptorPool());
	const vk::Unique<vk::VkDescriptorSet>			descriptorSet		(createDescriptorSet(*descriptorPool, *descriptorSetLayout));
	const ComputePipeline							pipeline			(m_vki, m_device, m_context.getBinaryCollection(), 1, &descriptorSetLayout.get());

	const vk::VkDescriptorSet						descriptorSets[]	= { *descriptorSet };
	const int										numDescriptorSets	= DE_LENGTH_OF_ARRAY(descriptorSets);
	const deUint32* const							dynamicOffsets		= DE_NULL;
	const int										numDynamicOffsets	= 0;
	const void*	const								preBarriers[]		= { m_texelBuffers.getBufferInitBarrierA(), m_texelBuffers.getBufferInitBarrierB() };
	const int										numPreBarriers		= m_texelBuffers.getNumTexelBuffers();
	const void* const								postBarriers[]		= { m_result.getResultReadBarrier() };
	const int										numPostBarriers		= DE_LENGTH_OF_ARRAY(postBarriers);

	const ComputeCommand							compute				(m_vki,
																		 m_device,
																		 pipeline.getPipeline(),
																		 pipeline.getPipelineLayout(),
																		 tcu::UVec3(4, 1, 1),
																		 numDescriptorSets,	descriptorSets,
																		 numDynamicOffsets,	dynamicOffsets,
																		 numPreBarriers,	preBarriers,
																		 numPostBarriers,	postBarriers);

	tcu::Vec4										results[4];
	bool											anyResultSet		= false;
	bool											allResultsOk		= true;

	compute.submitAndWait(m_queueFamilyIndex, m_queue);
	m_result.readResultContentsTo(&results);

	// verify
	for (int resultNdx = 0; resultNdx < 4; ++resultNdx)
	{
		const tcu::Vec4	result				= results[resultNdx];
		const tcu::Vec4	reference			= m_texelBuffers.fetchTexelValue(resultNdx);
		const tcu::Vec4	conversionThreshold	= tcu::Vec4(1.0f / 255.0f);

		if (result != tcu::Vec4(-1.0f))
			anyResultSet = true;

		if (tcu::boolAny(tcu::greaterThan(tcu::abs(result - reference), conversionThreshold)))
		{
			allResultsOk = false;

			m_context.getTestContext().getLog()
				<< tcu::TestLog::Message
				<< "Test sample " << resultNdx << ": Expected " << reference << ", got " << result
				<< tcu::TestLog::EndMessage;
		}
	}

	// read back and verify
	if (allResultsOk)
		return tcu::TestStatus::pass("Pass");
	else if (anyResultSet)
		return tcu::TestStatus::fail("Invalid result values");
	else
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Result buffer was not written to."
			<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Result buffer was not written to");
	}
}

class TexelBufferDescriptorCase : public QuadrantRendederCase
{
public:
	enum
	{
		FLAG_VIEW_OFFSET = (1u << 1u),
	};
	// enum continues where resource flags ends
	DE_STATIC_ASSERT((deUint32)FLAG_VIEW_OFFSET == (deUint32)RESOURCE_FLAG_LAST);

								TexelBufferDescriptorCase	(tcu::TestContext&		testCtx,
															 const char*			name,
															 const char*			description,
															 bool					isPrimaryCmdBuf,
															 vk::VkDescriptorType	descriptorType,
															 vk::VkShaderStageFlags	exitingStages,
															 vk::VkShaderStageFlags	activeStages,
															 ShaderInputInterface	shaderInterface,
															 deUint32				flags);

private:
	std::string					genExtensionDeclarations	(vk::VkShaderStage stage) const;
	std::string					genResourceDeclarations		(vk::VkShaderStage stage, int numUsedBindings) const;
	std::string					genResourceAccessSource		(vk::VkShaderStage stage) const;
	std::string					genNoAccessSource			(void) const;

	vkt::TestInstance*			createInstance				(vkt::Context& context) const;

	const bool					m_isPrimaryCmdBuf;
	const vk::VkDescriptorType	m_descriptorType;
	const ShaderInputInterface	m_shaderInterface;
	const bool					m_nonzeroViewOffset;
};

TexelBufferDescriptorCase::TexelBufferDescriptorCase (tcu::TestContext&			testCtx,
													  const char*				name,
													  const char*				description,
													  bool						isPrimaryCmdBuf,
													  vk::VkDescriptorType		descriptorType,
													  vk::VkShaderStageFlags	exitingStages,
													  vk::VkShaderStageFlags	activeStages,
													  ShaderInputInterface		shaderInterface,
													  deUint32					flags)
	: QuadrantRendederCase	(testCtx, name, description, glu::GLSL_VERSION_310_ES, exitingStages, activeStages)
	, m_isPrimaryCmdBuf		(isPrimaryCmdBuf)
	, m_descriptorType		(descriptorType)
	, m_shaderInterface		(shaderInterface)
	, m_nonzeroViewOffset	(((flags & FLAG_VIEW_OFFSET) != 0) ? (1u) : (0u))
{
}

std::string TexelBufferDescriptorCase::genExtensionDeclarations (vk::VkShaderStage stage) const
{
	DE_UNREF(stage);
	return "#extension GL_EXT_texture_buffer : require\n";
}

std::string TexelBufferDescriptorCase::genResourceDeclarations (vk::VkShaderStage stage, int numUsedBindings) const
{
	DE_UNREF(stage);

	const bool			isUniform		= isUniformDescriptorType(m_descriptorType);
	const char* const	storageType		= (isUniform) ? ("samplerBuffer ") : ("readonly imageBuffer ");
	const char* const	formatQualifier	= (isUniform) ? ("") : (", rgba8");

	switch (m_shaderInterface)
	{
		case SHADER_INPUT_SINGLE_DESCRIPTOR:
			return "layout(set = 0, binding = " + de::toString(numUsedBindings) + formatQualifier + ") uniform highp " + storageType + " u_texelBuffer;\n";

		case SHADER_INPUT_MULTIPLE_DESCRIPTORS:
			return "layout(set = 0, binding = " + de::toString(numUsedBindings) + formatQualifier + ") uniform highp " + storageType + " u_texelBufferA;\n"
				   "layout(set = 0, binding = " + de::toString(numUsedBindings+1) + formatQualifier + ") uniform highp " + storageType + " u_texelBufferB;\n";

		case SHADER_INPUT_DESCRIPTOR_ARRAY:
			return "layout(set = 0, binding = " + de::toString(numUsedBindings) + formatQualifier + ") uniform highp " + storageType + " u_texelBuffer[2];\n";

		default:
			DE_FATAL("Impossible");
			return "";
	}
}

std::string TexelBufferDescriptorCase::genResourceAccessSource (vk::VkShaderStage stage) const
{
	DE_UNREF(stage);

	const char* const	accessPostfixA	= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR)		? ("")
										: (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS)	? ("A")
										: (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY)		? ("[0]")
										: (DE_NULL);
	const char* const	accessPostfixB	= (m_shaderInterface == SHADER_INPUT_SINGLE_DESCRIPTOR)		? ("")
										: (m_shaderInterface == SHADER_INPUT_MULTIPLE_DESCRIPTORS)	? ("B")
										: (m_shaderInterface == SHADER_INPUT_DESCRIPTOR_ARRAY)		? ("[1]")
										: (DE_NULL);
	const char* const	fetchFunc		= (isUniformDescriptorType(m_descriptorType)) ? ("texelFetch") : ("imageLoad");
	std::ostringstream	buf;

	buf << "	if (quadrant_id == 0)\n"
		<< "		result_color = " << fetchFunc << "(u_texelBuffer" << accessPostfixA << ", " << TexelBufferInstanceBuffers::getFetchPos(0) << ");\n"
		<< "	else if (quadrant_id == 1)\n"
		<< "		result_color = " << fetchFunc << "(u_texelBuffer" << accessPostfixB << ", " << TexelBufferInstanceBuffers::getFetchPos(1) << ");\n"
		<< "	else if (quadrant_id == 2)\n"
		<< "		result_color = " << fetchFunc << "(u_texelBuffer" << accessPostfixA << ", " << TexelBufferInstanceBuffers::getFetchPos(2) << ");\n"
		<< "	else\n"
		<< "		result_color = " << fetchFunc << "(u_texelBuffer" << accessPostfixB << ", " << TexelBufferInstanceBuffers::getFetchPos(3) << ");\n";

	return buf.str();
}

std::string TexelBufferDescriptorCase::genNoAccessSource (void) const
{
	return "	if (quadrant_id == 1 || quadrant_id == 2)\n"
			"		result_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
			"	else\n"
			"		result_color = vec4(1.0, 1.0, 0.0, 1.0);\n";
}

vkt::TestInstance* TexelBufferDescriptorCase::createInstance (vkt::Context& context) const
{
	if (m_exitingStages == vk::VK_SHADER_STAGE_COMPUTE_BIT)
	{
		DE_ASSERT(m_isPrimaryCmdBuf); // secondaries are only valid within renderpass
		return new TexelBufferComputeInstance(context, m_descriptorType, m_shaderInterface, m_nonzeroViewOffset);
	}
	else
		return new TexelBufferRenderInstance(context, m_isPrimaryCmdBuf, m_descriptorType, m_activeStages, m_shaderInterface, m_nonzeroViewOffset);
}

void createShaderAccessImageTests (tcu::TestCaseGroup*		group,
								   bool						isPrimaryCmdBuf,
								   vk::VkDescriptorType		descriptorType,
								   vk::VkShaderStageFlags	exitingStages,
								   vk::VkShaderStageFlags	activeStages,
								   ShaderInputInterface		dimension,
								   deUint32					resourceFlags)
{
	static const struct
	{
		vk::VkImageViewType	viewType;
		const char*			name;
		const char*			description;
		deUint32			flags;
	} s_imageTypes[] =
	{
		{ vk::VK_IMAGE_VIEW_TYPE_1D,			"1d",						"1D image view",								0u										},
		{ vk::VK_IMAGE_VIEW_TYPE_1D,			"1d_base_mip",				"1D image subview with base mip level",			ImageDescriptorCase::FLAG_BASE_MIP		},
		{ vk::VK_IMAGE_VIEW_TYPE_1D,			"1d_base_slice",			"1D image subview with base array slice",		ImageDescriptorCase::FLAG_BASE_SLICE	},

		{ vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY,		"1d_array",					"1D array image view",							0u										},
		{ vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY,		"1d_array_base_mip",		"1D array image subview with base mip level",	ImageDescriptorCase::FLAG_BASE_MIP		},
		{ vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY,		"1d_array_base_slice",		"1D array image subview with base array slice",	ImageDescriptorCase::FLAG_BASE_SLICE	},

		{ vk::VK_IMAGE_VIEW_TYPE_2D,			"2d",						"2D image view",								0u										},
		{ vk::VK_IMAGE_VIEW_TYPE_2D,			"2d_base_mip",				"2D image subview with base mip level",			ImageDescriptorCase::FLAG_BASE_MIP		},
		{ vk::VK_IMAGE_VIEW_TYPE_2D,			"2d_base_slice",			"2D image subview with base array slice",		ImageDescriptorCase::FLAG_BASE_SLICE	},

		{ vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY,		"2d_array",					"2D array image view",							0u										},
		{ vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY,		"2d_array_base_mip",		"2D array image subview with base mip level",	ImageDescriptorCase::FLAG_BASE_MIP		},
		{ vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY,		"2d_array_base_slice",		"2D array image subview with base array slice",	ImageDescriptorCase::FLAG_BASE_SLICE	},

		{ vk::VK_IMAGE_VIEW_TYPE_3D,			"3d",						"3D image view",								0u										},
		{ vk::VK_IMAGE_VIEW_TYPE_3D,			"3d_base_mip",				"3D image subview with base mip level",			ImageDescriptorCase::FLAG_BASE_MIP		},
		// no 3d array textures

		{ vk::VK_IMAGE_VIEW_TYPE_CUBE,			"cube",						"Cube image view",								0u										},
		{ vk::VK_IMAGE_VIEW_TYPE_CUBE,			"cube_base_mip",			"Cube image subview with base mip level",		ImageDescriptorCase::FLAG_BASE_MIP		},
		{ vk::VK_IMAGE_VIEW_TYPE_CUBE,			"cube_base_slice",			"Cube image subview with base array slice",		ImageDescriptorCase::FLAG_BASE_SLICE	},

		{ vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	"cube_array",				"Cube image view",								0u										},
		{ vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	"cube_array_base_mip",		"Cube image subview with base mip level",		ImageDescriptorCase::FLAG_BASE_MIP		},
		{ vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	"cube_array_base_slice",	"Cube image subview with base array slice",		ImageDescriptorCase::FLAG_BASE_SLICE	},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_imageTypes); ++ndx)
	{
		// never overlap
		DE_ASSERT((s_imageTypes[ndx].flags & resourceFlags) == 0u);

		group->addChild(new ImageDescriptorCase(group->getTestContext(),
												s_imageTypes[ndx].name,
												s_imageTypes[ndx].description,
												isPrimaryCmdBuf,
												descriptorType,
												exitingStages,
												activeStages,
												dimension,
												s_imageTypes[ndx].viewType,
												s_imageTypes[ndx].flags | resourceFlags));
	}
}

void createShaderAccessTexelBufferTests (tcu::TestCaseGroup*	group,
										 bool					isPrimaryCmdBuf,
										 vk::VkDescriptorType	descriptorType,
										 vk::VkShaderStageFlags	exitingStages,
										 vk::VkShaderStageFlags	activeStages,
										 ShaderInputInterface	dimension,
										 deUint32				resourceFlags)
{
	DE_ASSERT(resourceFlags == 0);
	DE_UNREF(resourceFlags);

	static const struct
	{
		const char*	name;
		const char*	description;
		deUint32	flags;
	} s_texelBufferTypes[] =
	{
		{ "offset_zero",		"View offset is zero",		0u											},
		{ "offset_nonzero",		"View offset is non-zero",	TexelBufferDescriptorCase::FLAG_VIEW_OFFSET	},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_texelBufferTypes); ++ndx)
	{
		group->addChild(new TexelBufferDescriptorCase(group->getTestContext(),
													  s_texelBufferTypes[ndx].name,
													  s_texelBufferTypes[ndx].description,
													  isPrimaryCmdBuf,
													  descriptorType,
													  exitingStages,
													  activeStages,
													  dimension,
													  s_texelBufferTypes[ndx].flags));
	}
}

void createShaderAccessBufferTests (tcu::TestCaseGroup*		group,
									bool					isPrimaryCmdBuf,
									vk::VkDescriptorType	descriptorType,
									vk::VkShaderStageFlags	exitingStages,
									vk::VkShaderStageFlags	activeStages,
									ShaderInputInterface	dimension,
									deUint32				resourceFlags)
{
	DE_ASSERT(resourceFlags == 0u);
	DE_UNREF(resourceFlags);

	static const struct
	{
		const char*	name;
		const char*	description;
		bool		isForDynamicCases;
		deUint32	flags;
	} s_bufferTypes[] =
	{
		{ "offset_view_zero",						"View offset is zero",									false,	0u																							},
		{ "offset_view_nonzero",					"View offset is non-zero",								false,	BufferDescriptorCase::FLAG_VIEW_OFFSET														},

		{ "offset_view_zero_dynamic_zero",			"View offset is zero, dynamic offset is zero",			true,	BufferDescriptorCase::FLAG_DYNAMIC_OFFSET_ZERO												},
		{ "offset_view_zero_dynamic_nonzero",		"View offset is zero, dynamic offset is non-zero",		true,	BufferDescriptorCase::FLAG_DYNAMIC_OFFSET_NONZERO											},
		{ "offset_view_nonzero_dynamic_zero",		"View offset is non-zero, dynamic offset is zero",		true,	BufferDescriptorCase::FLAG_VIEW_OFFSET | BufferDescriptorCase::FLAG_DYNAMIC_OFFSET_ZERO		},
		{ "offset_view_nonzero_dynamic_nonzero",	"View offset is non-zero, dynamic offset is non-zero",	true,	BufferDescriptorCase::FLAG_VIEW_OFFSET | BufferDescriptorCase::FLAG_DYNAMIC_OFFSET_NONZERO	},
	};

	const bool isDynamicCase = isDynamicDescriptorType(descriptorType);

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_bufferTypes); ++ndx)
	{
		if (isDynamicCase == s_bufferTypes[ndx].isForDynamicCases)
			group->addChild(new BufferDescriptorCase(group->getTestContext(),
													 s_bufferTypes[ndx].name,
													 s_bufferTypes[ndx].description,
													 isPrimaryCmdBuf,
													 descriptorType,
													 exitingStages,
													 activeStages,
													 dimension,
													 s_bufferTypes[ndx].flags));
	}
}

} // anonymous

tcu::TestCaseGroup* createShaderAccessTests (tcu::TestContext& testCtx)
{
	static const struct
	{
		const bool	isPrimary;
		const char*	name;
		const char*	description;
	} s_bindTypes[] =
	{
		{ true,		"primary_cmd_buf",	"Bind in primary command buffer"	},
		{ false,	"seconday_cmd_buf",	"Bind in secondary command buffer"	},
	};
	static const struct
	{
		const vk::VkDescriptorType	descriptorType;
		const char*					name;
		const char*					description;
		deUint32					flags;
	} s_descriptorTypes[] =
	{
		{ vk::VK_DESCRIPTOR_TYPE_SAMPLER,					"sampler_mutable",					"VK_DESCRIPTOR_TYPE_SAMPLER with mutable sampler",					0u								},
		{ vk::VK_DESCRIPTOR_TYPE_SAMPLER,					"sampler_immutable",				"VK_DESCRIPTOR_TYPE_SAMPLER with immutable sampler",				RESOURCE_FLAG_IMMUTABLE_SAMPLER	},
		{ vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	"combined_image_sampler_mutable",	"VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER with mutable sampler",	0u								},
		{ vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	"combined_image_sampler_immutable",	"VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER with immutable sampler",	RESOURCE_FLAG_IMMUTABLE_SAMPLER	},
		// \note No way to access SAMPLED_IMAGE without a sampler
//		{ vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				"sampled_image",					"VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE",									0u								},
		{ vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				"storage_image",					"VK_DESCRIPTOR_TYPE_STORAGE_IMAGE",									0u								},
		{ vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,		"uniform_texel_buffer",				"VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER",							0u								},
		{ vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,		"storage_texel_buffer",				"VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER",							0u								},
		{ vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			"uniform_buffer",					"VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER",								0u								},
		{ vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			"storage_buffer",					"VK_DESCRIPTOR_TYPE_STORAGE_BUFFER",								0u								},
		{ vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,	"uniform_buffer_dynamic",			"VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",						0u								},
		{ vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,	"storage_buffer_dynamic",			"VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC",						0u								},
	};
	static const struct
	{
		const char*				name;
		const char*				description;
		vk::VkShaderStageFlags	existingStages;				//!< stages that exists
		vk::VkShaderStageFlags	activeStages;				//!< stages that access resource
		bool					supportsSecondaryCmdBufs;
	} s_shaderStages[] =
	{
		{
			"no_access",
			"No accessing stages",
			vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			0u,
			true,
		},
		{
			"vertex",
			"Vertex stage",
			vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			vk::VK_SHADER_STAGE_VERTEX_BIT,
			true,
		},
		{
			"tess_ctrl",
			"Tessellation control stage",
			vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_TESS_CONTROL_BIT | vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			vk::VK_SHADER_STAGE_TESS_CONTROL_BIT,
			true,
		},
		{
			"tess_eval",
			"Tessellation evaluation stage",
			vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_TESS_CONTROL_BIT | vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			vk::VK_SHADER_STAGE_TESS_EVALUATION_BIT,
			true,
		},
		{
			"geometry",
			"Geometry stage",
			vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_GEOMETRY_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			vk::VK_SHADER_STAGE_GEOMETRY_BIT,
			true,
		},
		{
			"fragment",
			"Fragment stage",
			vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			true,
		},
		{
			"compute",
			"Compute stage",
			vk::VK_SHADER_STAGE_COMPUTE_BIT,
			vk::VK_SHADER_STAGE_COMPUTE_BIT,
			false,
		},
		{
			"vertex_fragment",
			"Vertex and fragment stages",
			vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			true,
		},
	};
	static const struct
	{
		ShaderInputInterface	dimension;
		const char*				name;
		const char*				description;
	} s_variableDimensions[] =
	{
		{ SHADER_INPUT_SINGLE_DESCRIPTOR,		"single_descriptor",	"Single descriptor"		},
		{ SHADER_INPUT_MULTIPLE_DESCRIPTORS,	"multiple_descriptors",	"Multiple descriptors"	},
		{ SHADER_INPUT_DESCRIPTOR_ARRAY,		"descriptor_array",		"Descriptor array"		},
	};

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "shader_access", "Access resource via descriptor in a single descriptor set"));

	// .primary_cmd_buf...
	for (int bindTypeNdx = 0; bindTypeNdx < DE_LENGTH_OF_ARRAY(s_bindTypes); ++bindTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> bindGroup(new tcu::TestCaseGroup(testCtx, s_bindTypes[bindTypeNdx].name, s_bindTypes[bindTypeNdx].description));

		// .sampler, .combined_image_sampler, other resource types ...
		for (int descriptorNdx = 0; descriptorNdx < DE_LENGTH_OF_ARRAY(s_descriptorTypes); ++descriptorNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> typeGroup(new tcu::TestCaseGroup(testCtx, s_descriptorTypes[descriptorNdx].name, s_descriptorTypes[descriptorNdx].description));

			for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(s_shaderStages); ++stageNdx)
			{
				if (s_bindTypes[bindTypeNdx].isPrimary || s_shaderStages[stageNdx].supportsSecondaryCmdBufs)
				{
					de::MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, s_shaderStages[stageNdx].name, s_shaderStages[stageNdx].description));

					for (int dimensionNdx = 0; dimensionNdx < DE_LENGTH_OF_ARRAY(s_variableDimensions); ++dimensionNdx)
					{
						de::MovePtr<tcu::TestCaseGroup>	dimensionGroup(new tcu::TestCaseGroup(testCtx, s_variableDimensions[dimensionNdx].name, s_variableDimensions[dimensionNdx].description));
						void							(*createTestsFunc)(tcu::TestCaseGroup*		group,
																		   bool						isPrimaryCmdBuf,
																		   vk::VkDescriptorType		descriptorType,
																		   vk::VkShaderStageFlags	existingStages,
																		   vk::VkShaderStageFlags	activeStages,
																		   ShaderInputInterface		dimension,
																		   deUint32					resourceFlags);

						switch (s_descriptorTypes[descriptorNdx].descriptorType)
						{
							case vk::VK_DESCRIPTOR_TYPE_SAMPLER:
							case vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
							case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
								createTestsFunc = createShaderAccessImageTests;
								break;

							case vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
							case vk::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
								createTestsFunc = createShaderAccessTexelBufferTests;
								break;

							case vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
							case vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
							case vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
							case vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
								createTestsFunc = createShaderAccessBufferTests;
								break;

							default:
								createTestsFunc = DE_NULL;
								DE_FATAL("Impossible");
						}

						if (createTestsFunc)
						{
							createTestsFunc(dimensionGroup.get(),
											s_bindTypes[bindTypeNdx].isPrimary,
											s_descriptorTypes[descriptorNdx].descriptorType,
											s_shaderStages[stageNdx].existingStages,
											s_shaderStages[stageNdx].activeStages,
											s_variableDimensions[dimensionNdx].dimension,
											s_descriptorTypes[descriptorNdx].flags);
						}
						else
							DE_FATAL("Impossible");

						stageGroup->addChild(dimensionGroup.release());
					}

					typeGroup->addChild(stageGroup.release());
				}
			}

			bindGroup->addChild(typeGroup.release());
		}

		group->addChild(bindGroup.release());
	}

	return group.release();
}

} // BindingModel
} // vkt