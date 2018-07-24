#include "pocl_util.h"
#include "pocl_shared.h"
#include "pocl_image_util.h"

CL_API_ENTRY cl_int CL_API_CALL
POname(clEnqueueCopyImageToBuffer)(cl_command_queue  command_queue ,
                           cl_mem            src_image ,
                           cl_mem            dst_buffer ,
                           const size_t *    src_origin ,
                           const size_t *    region ,
                           size_t            dst_offset ,
                           cl_uint           num_events_in_wait_list ,
                           const cl_event *  event_wait_list ,
                           cl_event *        event ) CL_API_SUFFIX__VERSION_1_0
{
  _cl_command_node *cmd = NULL;
  /* pass dst_origin through in a format pocl_rect_copy understands */
  const size_t dst_origin[3] = { dst_offset, 0, 0};

  POCL_RETURN_ERROR_COND ((src_image == NULL), CL_INVALID_MEM_OBJECT);

  cl_int err = pocl_rect_copy(
    command_queue,
    CL_COMMAND_COPY_IMAGE_TO_BUFFER,
    src_image, CL_TRUE,
    dst_buffer, CL_FALSE,
    src_origin, dst_origin, region,
    0, 0,
    0, 0,
    num_events_in_wait_list, event_wait_list,
    event,
    &cmd);

  if (err != CL_SUCCESS)
    return err;

  cl_device_id dev = command_queue->device;

  cmd->command.read_image.src_mem_id = &src_image->device_ptrs[dev->dev_id];
  cmd->command.read_image.dst_host_ptr = NULL;
  cmd->command.read_image.dst_mem_id = &dst_buffer->device_ptrs[dev->dev_id];

  cmd->command.read_image.origin[0] = src_origin[0];
  cmd->command.read_image.origin[1] = src_origin[1];
  cmd->command.read_image.origin[2] = src_origin[2];
  cmd->command.read_image.region[0] = region[0];
  cmd->command.read_image.region[1] = region[1];
  cmd->command.read_image.region[2] = region[2];
  // TODO
  cmd->command.read_image.dst_row_pitch = 0;   // src_image->image_row_pitch;
  cmd->command.read_image.dst_slice_pitch = 0; // src_image->image_slice_pitch;
  cmd->command.read_image.dst_offset = dst_offset;

  POname (clRetainMemObject) (src_image);
  src_image->owning_device = dev;
  POname (clRetainMemObject) (dst_buffer);
  dst_buffer->owning_device = dev;

  pocl_command_enqueue (command_queue, cmd);

  return CL_SUCCESS;
}
POsym(clEnqueueCopyImageToBuffer)


