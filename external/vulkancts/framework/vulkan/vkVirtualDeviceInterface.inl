/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
virtual void		destroyDevice									(VkDevice device) const = 0;
virtual VkResult	getDeviceQueue									(VkDevice device, deUint32 queueFamilyIndex, deUint32 queueIndex, VkQueue* pQueue) const = 0;
virtual VkResult	queueSubmit										(VkQueue queue, deUint32 cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence) const = 0;
virtual VkResult	queueWaitIdle									(VkQueue queue) const = 0;
virtual VkResult	deviceWaitIdle									(VkDevice device) const = 0;
virtual VkResult	allocMemory										(VkDevice device, const VkMemoryAllocInfo* pAllocInfo, VkDeviceMemory* pMem) const = 0;
virtual void		freeMemory										(VkDevice device, VkDeviceMemory mem) const = 0;
virtual VkResult	mapMemory										(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) const = 0;
virtual void		unmapMemory										(VkDevice device, VkDeviceMemory mem) const = 0;
virtual VkResult	flushMappedMemoryRanges							(VkDevice device, deUint32 memRangeCount, const VkMappedMemoryRange* pMemRanges) const = 0;
virtual VkResult	invalidateMappedMemoryRanges					(VkDevice device, deUint32 memRangeCount, const VkMappedMemoryRange* pMemRanges) const = 0;
virtual VkResult	getDeviceMemoryCommitment						(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) const = 0;
virtual VkResult	bindBufferMemory								(VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset) const = 0;
virtual VkResult	bindImageMemory									(VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset) const = 0;
virtual VkResult	getBufferMemoryRequirements						(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) const = 0;
virtual VkResult	getImageMemoryRequirements						(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) const = 0;
virtual VkResult	getImageSparseMemoryRequirements				(VkDevice device, VkImage image, deUint32* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) const = 0;
virtual VkResult	getPhysicalDeviceSparseImageFormatProperties	(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, deUint32 samples, VkImageUsageFlags usage, VkImageTiling tiling, deUint32* pNumProperties, VkSparseImageFormatProperties* pProperties) const = 0;
virtual VkResult	queueBindSparseBufferMemory						(VkQueue queue, VkBuffer buffer, deUint32 numBindings, const VkSparseMemoryBindInfo* pBindInfo) const = 0;
virtual VkResult	queueBindSparseImageOpaqueMemory				(VkQueue queue, VkImage image, deUint32 numBindings, const VkSparseMemoryBindInfo* pBindInfo) const = 0;
virtual VkResult	queueBindSparseImageMemory						(VkQueue queue, VkImage image, deUint32 numBindings, const VkSparseImageMemoryBindInfo* pBindInfo) const = 0;
virtual VkResult	createFence										(VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence) const = 0;
virtual void		destroyFence									(VkDevice device, VkFence fence) const = 0;
virtual VkResult	resetFences										(VkDevice device, deUint32 fenceCount, const VkFence* pFences) const = 0;
virtual VkResult	getFenceStatus									(VkDevice device, VkFence fence) const = 0;
virtual VkResult	waitForFences									(VkDevice device, deUint32 fenceCount, const VkFence* pFences, VkBool32 waitAll, deUint64 timeout) const = 0;
virtual VkResult	createSemaphore									(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore) const = 0;
virtual void		destroySemaphore								(VkDevice device, VkSemaphore semaphore) const = 0;
virtual VkResult	queueSignalSemaphore							(VkQueue queue, VkSemaphore semaphore) const = 0;
virtual VkResult	queueWaitSemaphore								(VkQueue queue, VkSemaphore semaphore) const = 0;
virtual VkResult	createEvent										(VkDevice device, const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent) const = 0;
virtual void		destroyEvent									(VkDevice device, VkEvent event) const = 0;
virtual VkResult	getEventStatus									(VkDevice device, VkEvent event) const = 0;
virtual VkResult	setEvent										(VkDevice device, VkEvent event) const = 0;
virtual VkResult	resetEvent										(VkDevice device, VkEvent event) const = 0;
virtual VkResult	createQueryPool									(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool) const = 0;
virtual void		destroyQueryPool								(VkDevice device, VkQueryPool queryPool) const = 0;
virtual VkResult	getQueryPoolResults								(VkDevice device, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, deUintptr* pDataSize, void* pData, VkQueryResultFlags flags) const = 0;
virtual VkResult	createBuffer									(VkDevice device, const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer) const = 0;
virtual void		destroyBuffer									(VkDevice device, VkBuffer buffer) const = 0;
virtual VkResult	createBufferView								(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView) const = 0;
virtual void		destroyBufferView								(VkDevice device, VkBufferView bufferView) const = 0;
virtual VkResult	createImage										(VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage) const = 0;
virtual void		destroyImage									(VkDevice device, VkImage image) const = 0;
virtual VkResult	getImageSubresourceLayout						(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) const = 0;
virtual VkResult	createImageView									(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView) const = 0;
virtual void		destroyImageView								(VkDevice device, VkImageView imageView) const = 0;
virtual VkResult	createShaderModule								(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModule* pShaderModule) const = 0;
virtual void		destroyShaderModule								(VkDevice device, VkShaderModule shaderModule) const = 0;
virtual VkResult	createShader									(VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader) const = 0;
virtual void		destroyShader									(VkDevice device, VkShader shader) const = 0;
virtual VkResult	createPipelineCache								(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache) const = 0;
virtual void		destroyPipelineCache							(VkDevice device, VkPipelineCache pipelineCache) const = 0;
virtual deUintptr	getPipelineCacheSize							(VkDevice device, VkPipelineCache pipelineCache) const = 0;
virtual VkResult	getPipelineCacheData							(VkDevice device, VkPipelineCache pipelineCache, void* pData) const = 0;
virtual VkResult	mergePipelineCaches								(VkDevice device, VkPipelineCache destCache, deUint32 srcCacheCount, const VkPipelineCache* pSrcCaches) const = 0;
virtual VkResult	createGraphicsPipelines							(VkDevice device, VkPipelineCache pipelineCache, deUint32 count, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) const = 0;
virtual VkResult	createComputePipelines							(VkDevice device, VkPipelineCache pipelineCache, deUint32 count, const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) const = 0;
virtual void		destroyPipeline									(VkDevice device, VkPipeline pipeline) const = 0;
virtual VkResult	createPipelineLayout							(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout) const = 0;
virtual void		destroyPipelineLayout							(VkDevice device, VkPipelineLayout pipelineLayout) const = 0;
virtual VkResult	createSampler									(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler) const = 0;
virtual void		destroySampler									(VkDevice device, VkSampler sampler) const = 0;
virtual VkResult	createDescriptorSetLayout						(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout) const = 0;
virtual void		destroyDescriptorSetLayout						(VkDevice device, VkDescriptorSetLayout descriptorSetLayout) const = 0;
virtual VkResult	createDescriptorPool							(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool) const = 0;
virtual void		destroyDescriptorPool							(VkDevice device, VkDescriptorPool descriptorPool) const = 0;
virtual VkResult	resetDescriptorPool								(VkDevice device, VkDescriptorPool descriptorPool) const = 0;
virtual VkResult	allocDescriptorSets								(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, deUint32 count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets) const = 0;
virtual VkResult	freeDescriptorSets								(VkDevice device, VkDescriptorPool descriptorPool, deUint32 count, const VkDescriptorSet* pDescriptorSets) const = 0;
virtual void		updateDescriptorSets							(VkDevice device, deUint32 writeCount, const VkWriteDescriptorSet* pDescriptorWrites, deUint32 copyCount, const VkCopyDescriptorSet* pDescriptorCopies) const = 0;
virtual VkResult	createFramebuffer								(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer) const = 0;
virtual void		destroyFramebuffer								(VkDevice device, VkFramebuffer framebuffer) const = 0;
virtual VkResult	createRenderPass								(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass) const = 0;
virtual void		destroyRenderPass								(VkDevice device, VkRenderPass renderPass) const = 0;
virtual VkResult	getRenderAreaGranularity						(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) const = 0;
virtual VkResult	createCommandPool								(VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, VkCmdPool* pCmdPool) const = 0;
virtual void		destroyCommandPool								(VkDevice device, VkCmdPool cmdPool) const = 0;
virtual VkResult	resetCommandPool								(VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags) const = 0;
virtual VkResult	createCommandBuffer								(VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer) const = 0;
virtual void		destroyCommandBuffer							(VkDevice device, VkCmdBuffer commandBuffer) const = 0;
virtual VkResult	beginCommandBuffer								(VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo) const = 0;
virtual VkResult	endCommandBuffer								(VkCmdBuffer cmdBuffer) const = 0;
virtual VkResult	resetCommandBuffer								(VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags) const = 0;
virtual void		cmdBindPipeline									(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const = 0;
virtual void		cmdSetViewport									(VkCmdBuffer cmdBuffer, deUint32 viewportCount, const VkViewport* pViewports) const = 0;
virtual void		cmdSetScissor									(VkCmdBuffer cmdBuffer, deUint32 scissorCount, const VkRect2D* pScissors) const = 0;
virtual void		cmdSetLineWidth									(VkCmdBuffer cmdBuffer, float lineWidth) const = 0;
virtual void		cmdSetDepthBias									(VkCmdBuffer cmdBuffer, float depthBias, float depthBiasClamp, float slopeScaledDepthBias) const = 0;
virtual void		cmdSetBlendConstants							(VkCmdBuffer cmdBuffer, const float blendConst) const = 0;
virtual void		cmdSetDepthBounds								(VkCmdBuffer cmdBuffer, float minDepthBounds, float maxDepthBounds) const = 0;
virtual void		cmdSetStencilCompareMask						(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, deUint32 stencilCompareMask) const = 0;
virtual void		cmdSetStencilWriteMask							(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, deUint32 stencilWriteMask) const = 0;
virtual void		cmdSetStencilReference							(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, deUint32 stencilReference) const = 0;
virtual void		cmdBindDescriptorSets							(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, deUint32 firstSet, deUint32 setCount, const VkDescriptorSet* pDescriptorSets, deUint32 dynamicOffsetCount, const deUint32* pDynamicOffsets) const = 0;
virtual void		cmdBindIndexBuffer								(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const = 0;
virtual void		cmdBindVertexBuffers							(VkCmdBuffer cmdBuffer, deUint32 startBinding, deUint32 bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) const = 0;
virtual void		cmdDraw											(VkCmdBuffer cmdBuffer, deUint32 vertexCount, deUint32 instanceCount, deUint32 firstVertex, deUint32 firstInstance) const = 0;
virtual void		cmdDrawIndexed									(VkCmdBuffer cmdBuffer, deUint32 indexCount, deUint32 instanceCount, deUint32 firstIndex, deInt32 vertexOffset, deUint32 firstInstance) const = 0;
virtual void		cmdDrawIndirect									(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 count, deUint32 stride) const = 0;
virtual void		cmdDrawIndexedIndirect							(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, deUint32 count, deUint32 stride) const = 0;
virtual void		cmdDispatch										(VkCmdBuffer cmdBuffer, deUint32 x, deUint32 y, deUint32 z) const = 0;
virtual void		cmdDispatchIndirect								(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset) const = 0;
virtual void		cmdCopyBuffer									(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, deUint32 regionCount, const VkBufferCopy* pRegions) const = 0;
virtual void		cmdCopyImage									(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageCopy* pRegions) const = 0;
virtual void		cmdBlitImage									(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageBlit* pRegions, VkTexFilter filter) const = 0;
virtual void		cmdCopyBufferToImage							(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkBufferImageCopy* pRegions) const = 0;
virtual void		cmdCopyImageToBuffer							(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, deUint32 regionCount, const VkBufferImageCopy* pRegions) const = 0;
virtual void		cmdUpdateBuffer									(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const deUint32* pData) const = 0;
virtual void		cmdFillBuffer									(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, deUint32 data) const = 0;
virtual void		cmdClearColorImage								(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, deUint32 rangeCount, const VkImageSubresourceRange* pRanges) const = 0;
virtual void		cmdClearDepthStencilImage						(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, deUint32 rangeCount, const VkImageSubresourceRange* pRanges) const = 0;
virtual void		cmdClearColorAttachment							(VkCmdBuffer cmdBuffer, deUint32 colorAttachment, VkImageLayout imageLayout, const VkClearColorValue* pColor, deUint32 rectCount, const VkRect3D* pRects) const = 0;
virtual void		cmdClearDepthStencilAttachment					(VkCmdBuffer cmdBuffer, VkImageAspectFlags aspectMask, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, deUint32 rectCount, const VkRect3D* pRects) const = 0;
virtual void		cmdResolveImage									(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, deUint32 regionCount, const VkImageResolve* pRegions) const = 0;
virtual void		cmdSetEvent										(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask) const = 0;
virtual void		cmdResetEvent									(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask) const = 0;
virtual void		cmdWaitEvents									(VkCmdBuffer cmdBuffer, deUint32 eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, deUint32 memBarrierCount, const void* const* ppMemBarriers) const = 0;
virtual void		cmdPipelineBarrier								(VkCmdBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, deUint32 memBarrierCount, const void* const* ppMemBarriers) const = 0;
virtual void		cmdBeginQuery									(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 slot, VkQueryControlFlags flags) const = 0;
virtual void		cmdEndQuery										(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 slot) const = 0;
virtual void		cmdResetQueryPool								(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount) const = 0;
virtual void		cmdWriteTimestamp								(VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkDeviceSize destOffset) const = 0;
virtual void		cmdCopyQueryPoolResults							(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, deUint32 startQuery, deUint32 queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags) const = 0;
virtual void		cmdPushConstants								(VkCmdBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, deUint32 start, deUint32 length, const void* values) const = 0;
virtual void		cmdBeginRenderPass								(VkCmdBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkRenderPassContents contents) const = 0;
virtual void		cmdNextSubpass									(VkCmdBuffer cmdBuffer, VkRenderPassContents contents) const = 0;
virtual void		cmdEndRenderPass								(VkCmdBuffer cmdBuffer) const = 0;
virtual void		cmdExecuteCommands								(VkCmdBuffer cmdBuffer, deUint32 cmdBuffersCount, const VkCmdBuffer* pCmdBuffers) const = 0;