#define T4_USE_OMP 1
//#define USE_MKLDNN
#include "tensor4.h"
#include "StyleGAN.h"
#include "image_io.h"
#include "numpy-like-randn.h"
#include "zfp.h"



#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>

using namespace emscripten;
#endif


#ifndef __EMSCRIPTEN__
int main(int argc, char** argv)
{
	int layers = 9;

	int seed  = 5;
	float psi = 0.7f;
	int n     = 1;

	int random_seed = 0;
	int smooth_psi = 0;
	int smooth_z    = 0;
	int seed2       = 841;
	int start_index = 0;
	int seed1toN    = 0;
	std::string model_name = "StyleGAN_karras2019stylegan-ffhq-1024x1024.ct4";
	std::string model_path = "./";

	for (int i = 1; i < argc; i++)
	{
		if (std::string(argv[i]) == "--num")
		{
			n = atoi(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--psi")
		{
			psi = atof(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--seed")
		{
			seed = atoi(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--seed2")
		{
			seed2 = atoi(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--random_seed")
		{
			random_seed = atoi(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--smooth_psi")
		{
			smooth_psi = atoi(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--smooth_z")
		{
			smooth_z = atoi(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--start_index")
		{
			start_index = atoi(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--seed1toN")
		{
			seed1toN = atoi(argv[i + 1]);
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--model")
		{
			model_name = argv[i + 1];
			if (model_name == "a1")
			{
				layers = 8;
				model_name = "StyleGAN_2019-02-26-stylegan-faces-network-02048-016041.ct4";
			}
			if (model_name == "a2")
			{
				layers = 8;
				model_name = "StyleGAN_2019-03-08-stylegan-animefaces-network-02051-021980.ct4";
			}
			if (model_name == "a3")
			{
				layers = 8;
				model_name = "StyleGAN_2019-04-30-stylegan-danbooru2018-portraits-02095-066083.ct4";
			}
			if (model_name == "f")
			{
				layers = 9;
				model_name = "StyleGAN_karras2019stylegan-ffhq-1024x1024.ct4";
			}
			if (model_name == "c")
			{
				layers = 9;
				model_name = "StyleGAN_karras2019stylegan-celebahq-1024x1024.ct4";
			}
			i++;
			continue;
		}
		if (std::string(argv[i]) == "--model_path")
		{
			model_path = argv[i + 1];
			i++;
			continue;
		}
		//
		if (std::string(argv[i]) == "--layers")
		{
			layers = atoi(argv[i + 1]);
			i++;
			continue;
		}

		fprintf(stderr, "option error:%s\n", argv[i]);
		fprintf(stderr, "--layers layers_value\n");
		fprintf(stderr, "--num number of output image\n");
		fprintf(stderr, "--seed seed_value\n");
		fprintf(stderr, "--psi truncation_psi\n");
		fprintf(stderr, "--seed1toN 0 or 1\n");
		fprintf(stderr, "--random_seed 0 or 1\n");
		fprintf(stderr, "--smooth_alp 0 or 1\n");
		fprintf(stderr, "--smooth_z 0 or 1\n");
		fprintf(stderr, "--start_index start of output image index\n");
		exit(0);
	}
	
	auto model = StyleGANLoad((model_path+model_name).c_str(), layers);

	// if loading original
	// auto model = StyleGANLoad("StyleGAN_cat.t4", layers, false);


	if (seed1toN)
	{
		seed = 1;
	}
	numpy_like::RandomState rs = numpy_like::RandomState(seed);
	auto z = GenZ(rs);

	t4::tensor2f z1;
	t4::tensor2f z2;
	t4::tensor2f dz;

	if (smooth_z )
	{
		z1 = z;
		if (n <= 0)
		{
			fprintf(stderr, "error:output <= 0\n");
			exit(0);
		}
		numpy_like::RandomState rs1 = numpy_like::RandomState(seed2);
		z2 = GenZ(rs1);
		dz = (z2 - z) / (float)(n - 1);
	}

	int seed_time = -1;
	for ( int k = 0; k < n; k++)
	{
		if (seed1toN)
		{
			seed = k;
			rs = numpy_like::RandomState(k);
			z = GenZ(rs);
		}
		if (random_seed)
		{
			seed_time = time(NULL);
			rs = numpy_like::RandomState(seed_time);
			z = GenZ(rs);
		}
		if (smooth_psi)
		{
			psi = -1.0f + 2.0f*(float)k / (float)(abs(n)-1);
		}

		if (smooth_z)
		{
			z = z1 + dz*(float)k;
		}

		auto w = GenW(model, z);
		auto w_truncated = (w - t4::Unsqueeze<0>(model.dlatent_avg)) * psi + t4::Unsqueeze<0>(model.dlatent_avg);
		t4::tensor4f x;
		t4::tensor3f img;
		for (int i = 0; i < layers; ++i)
		{
			t4::tensor2f current_w = w;
			if (i < 4)
			{
				current_w = w_truncated;
			}
			auto result = GenImage(model, x, current_w, i);
			x = result.first;
			img = result.second;

			//char imgfile[256];
			//sprintf(imgfile, "image_%d.png", i);
			//image_io::imwrite(img * 0.5f + 0.5f, imgfile);
		}
		//image_io::imwrite(img * 0.5f + 0.5f, "image_12.png");
		char imgfile[256];
		sprintf(imgfile, "image_%04d.png", k+ start_index);
		image_io::imwrite(img * 0.5f + 0.5f, imgfile);

		sprintf(imgfile, "output/image_%d_%.3f.png", (seed_time < 0)? seed: seed_time, psi);
		image_io::imwrite(img * 0.5f + 0.5f, imgfile);
	}

	return 0;
}
#endif

class Generator
{
public:
	Generator():rng(std::random_device()())
	{
		model = StyleGANLoad("StyleGAN.ct4", 9);
		x = t4::tensor4f();
	}

	std::string RandomZ()
	{
		z = GenZ(rng);
		return image_io::base64_encode((uint8_t*)z.ptr(), 512 * sizeof(float));
	}

	std::string RandomZfromASeed(uint32_t seed)
	{
		rng = numpy_like::RandomState(seed);
		return RandomZ();
	}

	void SetZfromString(const std::string& s)
	{
		z = t4::tensor2f::New({1, 512});
		size_t _;
		image_io::base64_decode(s.c_str(), (uint8_t*)z.ptr(), s.size(), _);
	}

	std::string GenerateImage()
	{
		if (step == 0)
		{
			w = GenW(model, z);
			w_truncated = (w - t4::Unsqueeze<0>(model.dlatent_avg)) * 0.7f + t4::Unsqueeze<0>(model.dlatent_avg);
		}

		t4::tensor2f current_w = w;
		if (step < 4)
		{
			current_w = w_truncated;
		}

		auto result = GenImage(model, x, current_w, step);
		step++;
		if (step== 9)
		{
			step = 0;
		}

		x = result.first;
		std::string image_png = image_io::imwrite_to_base64(result.second * 0.5f + 0.5f);
		return image_png;
	}

private:
	numpy_like::RandomState rng;
	StyleGAN model;
	t4::tensor4f x;
	t4::tensor2f z;
	t4::tensor2f w;
	t4::tensor2f w_truncated;
	int step = 0;
};

#ifdef __EMSCRIPTEN__
// Binding code
EMSCRIPTEN_BINDINGS(StyleGan) {
  class_<Generator>("Generator")
    .constructor<>()
    .function("GenerateImage", &Generator::GenerateImage)
    .function("RandomZ", &Generator::RandomZ)
    .function("RandomZfromASeed", &Generator::RandomZfromASeed)
    .function("SetZfromString", &Generator::SetZfromString)
    ;
}
#endif
