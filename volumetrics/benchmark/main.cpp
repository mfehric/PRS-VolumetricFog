#include "Workload.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{
    using Clock = std::chrono::steady_clock;
    struct Options
    {
        int iterations = 100, warmup = 3, width = 240, height = 180, shadowSize = 1024;
        VolumetricSettings settings;
        std::string csv;
    };

    template<class T> T Number(std::string_view text, T lo, T hi)
    {
        T value{};
        const auto [end,error] = std::from_chars(text.data(),text.data()+text.size(),value);
        if (error != std::errc{} || end != text.data()+text.size() || value < lo || value > hi)
            throw std::invalid_argument("Invalid number: "+std::string(text));
        if constexpr (std::is_floating_point_v<T>)
            if (!std::isfinite(value)) throw std::invalid_argument("Expected a finite number");
        return value;
    }

    Options Parse(int argc, char** argv)
    {
        Options options;
        for (int i=1; i<argc; ++i)
        {
            const std::string_view key = argv[i];
            if (i+1 == argc) throw std::invalid_argument("Missing value for "+std::string(key));
            const std::string_view value = argv[++i];
            if (key=="--iterations") options.iterations=Number<int>(value,1,10000000);
            else if (key=="--warmup") options.warmup=Number<int>(value,0,100000);
            else if (key=="--width") options.width=Number<int>(value,1,4096);
            else if (key=="--height") options.height=Number<int>(value,1,4096);
            else if (key=="--steps") options.settings.steps=Number<int>(value,1,256);
            else if (key=="--shadow-size") options.shadowSize=Number<int>(value,1,2048);
            else if (key=="--shadows") options.settings.shadows=Number<int>(value,0,1)!=0;
            else if (key=="--density") options.settings.density=Number<float>(value,0,10);
            else if (key=="--albedo") options.settings.scatteringAlbedo=Number<float>(value,0,1);
            else if (key=="--anisotropy") options.settings.anisotropy=Number<float>(value,-0.9f,0.9f);
            else if (key=="--max-distance") options.settings.maxDistance=Number<float>(value,0.001f,1000);
            else if (key=="--csv") options.csv=value;
            else throw std::invalid_argument("Unknown option: "+std::string(key));
        }
        options.settings.enabled=true;
        options.settings.backend=VolumetricBackend::CPU;
        options.settings.width=options.width;
        return options;
    }

    void Help()
    {
        std::cout << "VolumetricBenchmark: sequential CPU integration, no OpenGL\n"
            "--iterations N (100) --warmup N (3) --width N (240) --height N (180)\n"
            "--steps N (64) --shadow-size N (1024; four cascades) --shadows 0|1 (1)\n"
            "--density F (0.04) --albedo F (0.9) --anisotropy F (0.2)\n"
            "--max-distance F (50) --csv path (optional per-iteration results)\n"
            "Setup, warmup, checksums and CSV writes are outside kernel timing.\n";
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc==2 && std::string_view(argv[1])=="--help") { Help(); return 0; }
        const auto options = Parse(argc,argv);
        const auto setupStart = Clock::now();
        auto workload = benchmark::MakeWindowRoom(options.width,options.height,options.shadowSize);
        const auto setupMs = std::chrono::duration<double,std::milli>(Clock::now()-setupStart).count();
        auto integrate = [&] {
            VolumetricCPU::Integrate(workload.frame, options.settings, options.width, options.height,
                workload.sceneDepth, workload.shadowDepth, workload.output);
        };
        for (int i=0; i<options.warmup; ++i) integrate();
        std::vector<double> timings(options.iterations), checksums(options.iterations);
        std::cout << std::fixed << std::setprecision(6)
            << "Workload: window-room-v1 (deterministic), " << options.width << 'x' << options.height
            << ", steps=" << options.settings.steps << ", shadows=" << options.settings.shadows
            << ", shadow maps=4x" << options.shadowSize << 'x' << options.shadowSize << '\n'
            << "density=" << options.settings.density << ", albedo=" << options.settings.scatteringAlbedo
            << ", g=" << options.settings.anisotropy << ", max_distance=" << options.settings.maxDistance
            << ", iterations=" << options.iterations << ", warmup=" << options.warmup << '\n'
            << "Setup ms (excluded): " << setupMs << '\n' << std::flush;
        for (int i=0; i<options.iterations; ++i)
        {
            const auto start=Clock::now();
            integrate(); // Imported from VolumetricCPU.dll; no LTO across this boundary.
            timings[i]=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
            // Consume EVERY output after EVERY call. Includes a finite-value check.
            // Kept out of the reported integration timing, but visible in a profiler.
            checksums[i]=benchmark::Checksum(workload.output);
            if (!std::isfinite(checksums[i])) throw std::runtime_error("Non-finite output");
            if (i && checksums[i]!=checksums[0]) throw std::runtime_error("Non-deterministic output");
        }
        const double total=std::accumulate(timings.begin(),timings.end(),0.0);
        auto sorted=timings;
        std::sort(sorted.begin(),sorted.end());
        const double median=(sorted[(sorted.size()-1)/2]+sorted[sorted.size()/2])*0.5;
        const auto sample=workload.output[workload.output.size()/2];
        std::cout << "Kernel total ms: " << total << ", mean ms: " << total/options.iterations
            << ", median ms: " << median << ", min ms: " << sorted.front() << ", max ms: " << sorted.back()
            << "\nPixel-samples/second: " << double(options.width)*options.height*options.settings.steps*options.iterations/(total/1000)
            << "\nChecksum/frame: " << std::setprecision(12) << checksums[0]
            << ", checksum/all: " << std::accumulate(checksums.begin(),checksums.end(),0.0)
            << "\nSample (L.r,L.g,L.b,T): " << sample.x << ',' << sample.y << ',' << sample.z << ',' << sample.w << '\n';
        if (!options.csv.empty())
        {
            std::ofstream csv(options.csv);
            if (!csv) throw std::runtime_error("Cannot open CSV: "+options.csv);
            csv << "iteration,kernel_ms,checksum,width,height,steps,shadow_size,shadows,density,albedo,g,max_distance\n" << std::setprecision(17);
            for (int i=0; i<options.iterations; ++i)
                csv << i+1 << ',' << timings[i] << ',' << checksums[i] << ',' << options.width << ',' << options.height
                    << ',' << options.settings.steps << ',' << options.shadowSize << ',' << options.settings.shadows
                    << ',' << options.settings.density << ',' << options.settings.scatteringAlbedo << ','
                    << options.settings.anisotropy << ',' << options.settings.maxDistance << '\n';
            if (!csv) throw std::runtime_error("CSV write failed");
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "VolumetricBenchmark: " << error.what() << "\nUse --help for options.\n";
        return 1;
    }
}
