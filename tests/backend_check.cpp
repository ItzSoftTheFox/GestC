#include <opencv2/dnn.hpp>
#include <opencv2/core/ocl.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
cv::dnn::Net load(const std::string& file, int backend, int target) {
#if CV_VERSION_MAJOR >= 5
    auto net = cv::dnn::readNetFromTFLite(file, cv::dnn::ENGINE_CLASSIC);
#else
    auto net = cv::dnn::readNetFromTFLite(file);
#endif
    net.setPreferableBackend(backend); net.setPreferableTarget(target);
    return net;
}
void verify(const std::vector<cv::Mat>& outputs, const std::vector<size_t>& sizes) {
    if (outputs.size() != sizes.size()) throw std::runtime_error("Missing named outputs");
    for (size_t i = 0; i < sizes.size(); ++i)
        if (outputs[i].total() != sizes[i] || outputs[i].type() != CV_32F || !cv::checkRange(outputs[i]))
            throw std::runtime_error("Invalid tensor shape, type or values");
}
size_t occurrences(const std::string& text, const std::string& token) {
    size_t count = 0, pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) { ++count; pos += token.size(); }
    return count;
}
}
// Opt-in diagnostic, not a GPU selector. Use an external timeout for driver hangs.
int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "backend_check MODEL_DIR cpu|opencl|opencl-fp16|cuda|vulkan\n"; return 2; }
    const std::string name = argv[2];
    int backend = cv::dnn::DNN_BACKEND_OPENCV, target = cv::dnn::DNN_TARGET_CPU;
    if (name == "opencl") target = cv::dnn::DNN_TARGET_OPENCL;
    else if (name == "opencl-fp16") target = cv::dnn::DNN_TARGET_OPENCL_FP16;
    else if (name == "cuda") { backend = cv::dnn::DNN_BACKEND_CUDA; target = cv::dnn::DNN_TARGET_CUDA; }
    else if (name == "vulkan") { backend = cv::dnn::DNN_BACKEND_VKCOM; target = cv::dnn::DNN_TARGET_VULKAN; }
    else if (name != "cpu") return 2;
    try {
        cv::setNumThreads(2);
        const auto available = cv::dnn::getAvailableTargets(static_cast<cv::dnn::Backend>(backend));
        if (std::find(available.begin(), available.end(), target) == available.end()) {
            std::cerr << name << " is not available in this OpenCV/runtime; no silent CPU fallback.\n"; return 3;
        }
        if (name.find("opencl") == 0) {
            cv::ocl::setUseOpenCL(true);
            std::cout << "OpenCL device: " << cv::ocl::Device::getDefault().name() << std::endl;
        }
        const std::vector<cv::String> names{"Identity", "Identity_1"};
        bool compatible = true;
        for (bool palm : {true, false}) {
            const auto file = std::string(argv[1]) + (palm ? "/hand_detector.tflite" : "/hand_landmarks_detector.tflite");
            const std::vector<size_t> sizes = palm ? std::vector<size_t>{2016 * 18, 2016} : std::vector<size_t>{63, 1};
            auto reference = load(file, cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
            auto candidate = load(file, backend, target);
            const int size = palm ? 192 : 224;
            cv::Mat input(size, size, CV_8UC3);
            cv::RNG rng(1234); rng.fill(input, cv::RNG::UNIFORM, 0, 256);
            const auto blob = cv::dnn::blobFromImage(input, 1.0 / 255, {}, {}, true);
            std::vector<cv::Mat> expected, actual;
            reference.setInput(blob); reference.forward(expected, names); verify(expected, sizes);
            for (auto& output : expected) output = output.clone();
            std::cout << (palm ? "palm" : "landmarks") << " warming " << name << std::endl;
            for (int i = 0; i < 3; ++i) { candidate.setInput(blob); candidate.forward(actual, names); verify(actual, sizes); }
            double maxError = 0;
            for (size_t i = 0; i < actual.size(); ++i) maxError = std::max(maxError, cv::norm(actual[i], expected[i], cv::NORM_INF));
            const auto graph = candidate.dump();
            std::cout << "reported graph nodes CPU=" << occurrences(graph, "/CPU") << " OCL=" << occurrences(graph, "/OCL")
                      << " CUDA=" << occurrences(graph, "CUDA") << " Vulkan=" << occurrences(graph, "VKCOM") << '\n';
            // dump describes the selected graph; runtime kernels can still fall back per operation.
            std::vector<double> timings;
            for (int i = 0; i < 30; ++i) {
                const auto start = std::chrono::steady_clock::now();
                candidate.setInput(blob); candidate.forward(actual, names);
                timings.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now() - start).count());
                verify(actual, sizes);
            }
            std::sort(timings.begin(), timings.end());
            double total = 0; for (double ms : timings) total += ms;
            std::cout << "mean " << total / timings.size() << " ms; p95 " << timings[28]
                      << " ms; max tensor error vs CPU " << maxError << std::endl;
            if (maxError > (name == "opencl-fp16" ? 1.0 : 0.05)) compatible = false;
        }
        std::cout << "Synthetic tensor check only; this does not certify hand tracking stability or GPU speedup.\n";
        return compatible ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
