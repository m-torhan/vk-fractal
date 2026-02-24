/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <format>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "gfx/vk_context.hpp"
#include "util/checks.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL dbg_cb(VkDebugUtilsMessageSeverityFlagBitsEXT,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT *cb,
                                             void *) {
    std::cerr << std::format("Vulkan: {}\n", cb->pMessage);
    return VK_FALSE;
}

static VkDebugUtilsMessengerEXT g_dbg_messenger = VK_NULL_HANDLE;

static void create_debug(VkInstance inst) {
    auto vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkCreateDebugUtilsMessengerEXT");
    if (!vkCreateDebugUtilsMessengerEXT) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT ci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = dbg_cb;

    vk_check(vkCreateDebugUtilsMessengerEXT(inst, &ci, nullptr, &g_dbg_messenger), "vkCreateDebugUtilsMessengerEXT");
}

static void destroy_debug(VkInstance inst) {
    auto vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkDestroyDebugUtilsMessengerEXT");
    if (vkDestroyDebugUtilsMessengerEXT && g_dbg_messenger) {
        vkDestroyDebugUtilsMessengerEXT(inst, g_dbg_messenger, nullptr);
    }
    g_dbg_messenger = VK_NULL_HANDLE;
}

QueueFamilyIndices VkContext::find_queue_families(VkPhysicalDevice dev) {
    QueueFamilyIndices out{};
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());

    for (uint32_t i = 0; i < count; i++) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            out.graphics = i;
        }

        VkBool32 present = VK_FALSE;
        vk_check(vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &present),
                 "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (present) {
            out.present = i;
        }

        if (out.complete()) {
            break;
        }
    }
    return out;
}

bool VkContext::is_device_suitable(VkPhysicalDevice dev) {
    auto q = find_queue_families(dev);
    return q.complete();
}

void VkContext::init(GLFWwindow *window) {
    // Instance
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "vk-fractal";
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName = "vk-fractal";
    app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfw_ext_count = 0;
    const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    std::vector<const char *> exts(glfw_exts, glfw_exts + glfw_ext_count);
    exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char *> layers;
#ifndef NDEBUG
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    ici.ppEnabledExtensionNames = exts.data();
    ici.enabledLayerCount = static_cast<uint32_t>(layers.size());
    ici.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    vk_check(vkCreateInstance(&ici, nullptr, &instance_), "vkCreateInstance");
#ifndef NDEBUG
    create_debug(instance_);
#endif

    // Surface
    vk_check(glfwCreateWindowSurface(instance_, window, nullptr, &surface_), "glfwCreateWindowSurface");

    // Pick physical device
    uint32_t dev_count = 0;
    vk_check(vkEnumeratePhysicalDevices(instance_, &dev_count, nullptr), "vkEnumeratePhysicalDevices(count)");
    if (dev_count == 0) {
        throw std::runtime_error("No Vulkan physical devices found");
    }

    std::vector<VkPhysicalDevice> devs(dev_count);
    vk_check(vkEnumeratePhysicalDevices(instance_, &dev_count, devs.data()), "vkEnumeratePhysicalDevices(list)");

    for (auto d : devs) {
        if (is_device_suitable(d)) {
            phys_ = d;
            break;
        }
    }
    if (!phys_) {
        throw std::runtime_error("No suitable Vulkan device found");
    }

    vkGetPhysicalDeviceProperties(phys_, &props_);
    qf_ = find_queue_families(phys_);

    // Logical device
    float prio = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;

    auto make_qci = [&](uint32_t family) {
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;
        return qci;
    };

    qcis.push_back(make_qci(qf_.graphics));
    if (qf_.present != qf_.graphics) {
        qcis.push_back(make_qci(qf_.present));
    }

    std::vector<const char *> dev_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkPhysicalDeviceFeatures feats{}; // keep minimal

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos = qcis.data();
    dci.enabledExtensionCount = static_cast<uint32_t>(dev_exts.size());
    dci.ppEnabledExtensionNames = dev_exts.data();
    dci.pEnabledFeatures = &feats;

    vk_check(vkCreateDevice(phys_, &dci, nullptr, &device_), "vkCreateDevice");

    vkGetDeviceQueue(device_, qf_.graphics, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, qf_.present, 0, &present_queue_);

    // Command pool (graphics)
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = qf_.graphics;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vk_check(vkCreateCommandPool(device_, &cpci, nullptr, &cmd_pool_), "vkCreateCommandPool");
}

void VkContext::init_imgui(GLFWwindow *window, VkRenderPass render_pass, uint32_t swapchain_image_count) {
    imgui_.init(window,
                instance_,
                phys_,
                device_,
                qf_.graphics,
                graphics_queue_,
                render_pass,
                swapchain_image_count,
                cmd_pool_);
}

void VkContext::shutdown() {
    if (device_) {
        vkDeviceWaitIdle(device_);
        if (cmd_pool_) {
            vkDestroyCommandPool(device_, cmd_pool_, nullptr);
        }
        vkDestroyDevice(device_, nullptr);
    }
#ifndef NDEBUG
    if (instance_) {
        destroy_debug(instance_);
    }
#endif
    if (surface_) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
    }

    cmd_pool_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
    surface_ = VK_NULL_HANDLE;
    instance_ = VK_NULL_HANDLE;
}
