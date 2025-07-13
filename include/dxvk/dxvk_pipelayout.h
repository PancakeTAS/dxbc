#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

#include "dxvk_hash.h"

#include "util_math.h"
#include "util_bit.h"
#include "util_flags.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkPipelineManager;

  /**
   * \brief Order-invariant atomic access operation
   *
   * Information used to optimize barriers when a resource
   * is accessed exlusively via order-invariant stores.
   */
  struct DxvkAccessOp {
    enum OpType : uint16_t {
      None      = 0x0u,
      Or        = 0x1u,
      And       = 0x2u,
      Xor       = 0x3u,
      Add       = 0x4u,
      IMin      = 0x5u,
      IMax      = 0x6u,
      UMin      = 0x7u,
      UMax      = 0x8u,

      StoreF    = 0xdu,
      StoreUi   = 0xeu,
      StoreSi   = 0xfu,
    };

    DxvkAccessOp() = default;
    DxvkAccessOp(OpType t)
    : op(uint16_t(t)) { }

    DxvkAccessOp(OpType t, uint16_t constant)
    : op(uint16_t(t) | (constant << 4u)) { }

    uint16_t op = 0u;

    bool operator == (const DxvkAccessOp& t) const { return op == t.op; }
    bool operator != (const DxvkAccessOp& t) const { return op != t.op; }

    template<typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    explicit operator T() const { return op; }
  };

  static_assert(sizeof(DxvkAccessOp) == sizeof(uint16_t));

  /**
   * \brief Descriptor flags
   */
  enum class DxvkDescriptorFlag : uint8_t {
    /** Resource is a plain (uniform) buffer, not a view */
    UniformBuffer   = 0u,
    /** Image resource may be be multisampled */
    Multisampled    = 1u,
    /** Resource is accessed via push data */
    PushData        = 2u,
  };

  using DxvkDescriptorFlags = Flags<DxvkDescriptorFlag>;


  /**
   * \brief Binding info
   *
   * Stores metadata for a single binding in
   * a given shader, or for the whole pipeline.
   */
  struct DxvkBindingInfo {
    /** Shader-defined descriptor set index */
    uint32_t set = 0u;
    /** Shader-defined binding index */
    uint32_t binding = 0u;
    /** Binding slot for the resource */
    uint32_t resourceIndex = 0u;
    /** Descriptor type */
    VkDescriptorType descriptorType  = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    /** Size of descriptor array */
    uint32_t descriptorCount = 1u;
    /** Image view type */
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    /** Access flags for the resource in the shader */
    VkAccessFlags access = 0u;
    /** Additional binding properties */
    DxvkDescriptorFlags flags = 0u;
    /** Order-invariant access type, if any */
    DxvkAccessOp accessOp = DxvkAccessOp::None;
    /** Byte offset of raw address or descriptor index within
     *  the shader's push data block. This will get remapped
     *  when chaining push constant blocks. */
    uint32_t blockOffset = 0u;
  };

  /**
   * \brief Push data block
   *
   * Maps to a shader-defined push constant range, which may
   * contain user-provided data or raw shader bindings.
   *
   * For graphics pipelines, there are two types of push data
   * blocks: Global data, which is available to all stages in
   * the pipeline, and per-stage data.
   */
  class DxvkPushDataBlock {

  public:

    // One shared block and one per shader stage
    constexpr static uint32_t MaxBlockCount = 6u;

    DxvkPushDataBlock() = default;

    DxvkPushDataBlock(
            VkShaderStageFlags        stages,
            uint32_t                  offset,
            uint32_t                  size,
            uint32_t                  alignment,
            uint64_t                  resourceMask)
    : m_stageMask   (uint16_t(stages)),
      m_alignment   (uint16_t(alignment)),
      m_offset      (uint16_t(offset)),
      m_size        (uint16_t(size)),
      m_resourceMask(uint64_t(resourceMask)) { }

    DxvkPushDataBlock(
            uint32_t                  offset,
            uint32_t                  size,
            uint32_t                  alignment,
            uint64_t                  resourceMask)
    : DxvkPushDataBlock(0u, offset, size, alignment, resourceMask) { }

    /**
     * \brief Queries stage mask
     * \returns Stage mask
     */
    VkShaderStageFlags getStageMask() const {
      return m_stageMask;
    }

    /**
     * \brief Checks whether the block is shared
     *
     * Any push constant block with more than
     * one stage flag is considered shared.
     * \returns \c true if the block is shared
     */
    bool isShared() const {
      return bool(m_stageMask & (m_stageMask - 1u));
    }

    /**
     * \brief Checks whether the block is empty
     * \returns \c true for an empty block
     */
    bool isEmpty() const {
      return !m_size;
    }

    /**
     * \brief Queries block size
     * \returns Data block size
     */
    uint32_t getSize() const {
      return m_size;
    }

    /**
     * \brief Queries required block alignment
     *
     * Will be at least 4 bytes, but may be higher
     * if the block stores 64-bit data types.
     * \returns Required data alignment
     */
    uint32_t getAlignment() const {
      return m_alignment;
    }

    /**
     * \brief Push data offset
     *
     * Depending on the context, this either contains the
     * shader-defined push constant offset, or the real
     * offset as defined in the final push constant layout.
     *
     * When remapping, the offsets of all push constant
     * block members within this range will be changed.
     *
     * The shared push constant block will always be
     * mapped to offset 0.
     * \returns Push data offset
     */
    uint32_t getOffset() const {
      return m_offset;
    }

    /**
     * \brief Queries mask of dwords used for resource data
     *
     * The dword corresponding to each set bit in the mask will
     * not be taken from userdata, but will instead contain a
     * resource index or address.
     *
     * Bit 0 corresponds to the first dword int the block.
     * \returns Resource mask
     */
    uint64_t getResourceDwordMask() const {
      return m_resourceMask;
    }

    /**
     * \brief Merges block with another
     *
     * Useful when dealing with shared push constant block
     * definitions coming in from multiple shaders.
     *
     * If neither block is empty, then the offsets of both
     * blocks must be identical; different shaders using
     * this block must agree on its layout.
     * \param [in] other The block to merge with
     */
    void merge(const DxvkPushDataBlock& other) {
      uint32_t oldOffset = m_offset;
      uint32_t newOffset = other.m_offset;

      m_stageMask    |= other.m_stageMask;
      m_alignment     = std::max(m_alignment, other.m_alignment);
      m_offset        = std::min(newOffset, m_size ? oldOffset : newOffset);
      m_size          = align(std::max(oldOffset + m_size, newOffset + other.m_size) - m_offset, m_alignment);

      // Preserve correct bit location of resource masks
      m_resourceMask <<= (oldOffset / sizeof(uint32_t));
      m_resourceMask |= other.m_resourceMask << (newOffset / sizeof(uint32_t));
      m_resourceMask >>= m_offset / sizeof(uint32_t);
    }

    /**
     * \brief Shifts block to a certain offset
     *
     * Useful when remapping push constant ranges.
     * \param [in] newOffset New block offset
     * \param [in] newSize New block size
     */
    void rebase(uint32_t newOffset, uint32_t newSize) {
      m_offset = newOffset;
      m_size = newSize;
    }

    /**
     * \brief Makes block absolute
     *
     * Changes the block to have an offset of 0 while keeping
     * all data in its place. The resulting block may thus
     * be larger than the original.
     */
    void makeAbsolute() {
      m_resourceMask <<= m_offset;
      m_size += m_offset;

      m_offset = 0u;
    }

    /**
     * \brief Checks for equality
     *
     * \param [in] other Block to compare to
     * \returns \c true if the two blocks are identical
     */
    bool eq(const DxvkPushDataBlock& other) const {
      return m_stageMask    == other.m_stageMask
          && m_alignment    == other.m_alignment
          && m_offset       == other.m_offset
          && m_size         == other.m_size
          && m_resourceMask == other.m_resourceMask;
    }

    /**
     * \brief Computes hash
     * \returns Hash value
     */
    size_t hash() const {
      DxvkHashState hash;
      hash.add(m_stageMask);
      hash.add(m_alignment);
      hash.add(m_offset);
      hash.add(m_size);
      hash.add(m_resourceMask);
      return hash;
    }

    /**
     * \brief Computes push data index for given stage mask
     *
     * If this is a shared or compute block, the index will
     * always be 0, otherwise it depends on the exact stage.
     * \param [in] stageMask Stage mask
     * \returns Push data block index
     */
    static uint32_t computeIndex(VkShaderStageFlags stageMask) {
      if (stageMask & VK_SHADER_STAGE_COMPUTE_BIT)
        return 0u;

      uint32_t remainder = stageMask & (stageMask - 1u);
      return remainder ? 0u : (bit::tzcnt(uint32_t(stageMask)) + 1u);
    }

  private:

    uint16_t  m_stageMask     = 0u;
    uint16_t  m_alignment     = 0u;
    uint16_t  m_offset        = 0u;
    uint16_t  m_size          = 0u;
    uint64_t  m_resourceMask  = 0u;

  };

  /**
   * \brief Shader resource binding info
   *
   * Stores the set and binding index for a given binding that is
   * used in a shader. Used to patch binding numbers as necessary.
   */
  class DxvkShaderBinding {

  public:

    DxvkShaderBinding() = default;

    DxvkShaderBinding(
            VkShaderStageFlags    stages,
            uint32_t              set,
            uint32_t              binding)
    : m_stages  (uint8_t(stages)),
      m_set     (uint8_t(set)),
      m_binding (uint16_t(binding)) { }

    /**
     * \brief Queries stage mask
     * \returns Stage mask
     */
    VkShaderStageFlags getStageMask() const {
      return VkShaderStageFlags(m_stages);
    }

    /**
     * \brief Queries set index
     * \returns Set index
     */
    uint32_t getSet() const {
      return m_set;
    }

    /**
     * \brief Queries binding index
     * \returns Binding index
     */
    uint32_t getBinding() const {
      return m_binding;
    }

    /**
     * \brief Checks for equality
     *
     * \param [in] other Other binding entry
     * \returns \c true if all properties match
     */
    bool eq(const DxvkShaderBinding& other) const {
      return m_stages   == other.m_stages
          && m_set      == other.m_set
          && m_binding  == other.m_binding;
    }

    /**
     * \brief Computes hash
     * \returns Hash
     */
    size_t hash() const {
      return size_t(m_stages)
          | (size_t(m_set) << 8)
          | (size_t(m_binding) << 16);
    }

  private:

    uint8_t   m_stages   = 0u;
    uint8_t   m_set      = 0u;
    uint16_t  m_binding  = 0u;

  };

}
