#pragma once

#include <liblava/block/render_pass.hpp>
#include <liblava/block/render_pipeline.hpp>

struct mypipeline {
    lava::render_pipeline::ptr pipeline;
    lava::pipeline_layout::ptr layout;

    void destroy() {
        pipeline->destroy();
        layout->destroy();
    }
};

mypipeline make_color_pipeline(lava::device_p& device,
                               VkDescriptorSet& descriptor_set,
                               lava::descriptor::ptr& descriptor,
                               lava::render_pass::ptr& render_pass);
