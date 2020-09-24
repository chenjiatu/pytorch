#pragma once

#include <ATen/native/vulkan/api/Common.h>
#include <ATen/native/vulkan/api/Cache.h>
#include <ATen/native/vulkan/api/Resource.h>

namespace at {
namespace native {
namespace vulkan {
namespace api {

//
// This struct defines caches of descriptor pools, and descriptor sets allocated
// from those pools, intended to minimize redundant object reconstructions or
// accelerate unavoidable memory allocations, both at the cost of extra memory
// consumption.
//
// A descriptor set is logically an array of descriptors, each of which
// references a resource (i.e. buffers and images), in turn telling the core
// executing the shader, where in GPU, or GPU-accessible system, memory the said
// resource resides.
//
// To accelerate creation of the descriptor sets, modern graphics APIs allocate
// them from a pool, more elaborately referred to as descriptor pools, which do
// need to be purged frequently _after_ none of the descriptors the pools contain
// is in use by the GPU.  Care must be taken that descriptors are not freed while
// they are in use by the pipeline, which considering the asynchronous nature of
// CPU-GPU interactions, can be anytime after the command is issued until it is
// fully executed by the GPU.
//
// As you can imagine, it is possible to have multiple descriptor pools, each of
// which is configured to house different types of descriptor sets with different
// allocation strategies. These descriptor pools themselves are fairly stable
// objects in that they theymself should not be created and destroyed frequently.
// That is the reason why we store them in a cache, which according to our usage
// of the term 'cache' in this implementatoin, is reserved for objects that are
// created infrequently and stabilize to a manageable number quickly over the
// lifetime of the program.
//
// Descriptor sets though, on the other hand, are allocated from pools which
// indeed does mean that the pools must be purged on a regular basis or else
// they will run out of free items.  Again, this is in line with our usage of
// the term 'pool' in this implementation which we use to refer to a container
// of objects that is allocated out of and is required to be frequently purged.
//
// It is important to point out that for performance reasons, we intentionally
// do not free the descriptor sets individually, and instead opt to purge the
// pool in its totality, even though Vulkan supports the former usage pattern
// as well.  This behavior is by design.
//

struct Descriptor final {
  //
  // Set
  //

  class Set final {
   public:
    Set(
        VkDevice device,
        VkDescriptorPool descriptor_pool,
        VkDescriptorSetLayout descriptor_set_layout);
    Set(const Set&) = delete;
    Set& operator=(const Set&) = delete;
    Set(Set&&) = default;
    Set& operator=(Set&&) = default;
    ~Set() = default;

    Set& bind(
        uint32_t binding,
        VkDescriptorType type,
        const Resource::Buffer& buffer);

    Set& bind(
        uint32_t binding,
        VkDescriptorType type,
        const Resource::Image& image);

    VkDescriptorSet handle() const;

   private:
    struct Stream final {
      uint32_t binding;
      VkDescriptorType type;
      union {
        VkDescriptorBufferInfo buffer;
        VkDescriptorImageInfo image;
      } info;
    };

    void update(const Stream& stream);

   private:
    VkDevice device_;
    VkDescriptorSet descriptor_set_;

    struct {
      c10::SmallVector<Stream, 8u> streams;
      mutable bool dirty;
    } bindings_;
  };

  //
  // Pool
  //

  class Pool final {
   public:
    explicit Pool(const GPU& gpu);
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&&) = default;
    Pool& operator=(Pool&&) = default;
    ~Pool() = default;

    Set allocate(VkDescriptorSetLayout descriptor_set_layout);
    void purge();

   private:
    VkDevice device_;
    Handle<VkDescriptorPool, VK_DELETER(DescriptorPool)> descriptor_pool_;
  } pool /* [thread_count] */;

  explicit Descriptor(const GPU& gpu)
    : pool(gpu) {
  }
};

} // namespace api
} // namespace vulkan
} // namespace native
} // namespace at
