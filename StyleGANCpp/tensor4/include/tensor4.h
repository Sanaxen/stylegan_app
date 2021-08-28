// Copyright 2018-2019 Stanislav Pidhorskyi. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ==============================================================================
#pragma once

#include <memory>
#include <array>
#include <string>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <map>
#include <random>

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <cmath>
//#define USE_MKL

#ifdef USE_MKL
#include <mkl_cblas.h>
#endif

#if !defined(T4_USE_OMP)
#if defined(_OPENMP)
#define T4_USE_OMP 1
#else
#define T4_USE_OMP 0
#endif
#endif

#if T4_USE_OMP
#include <omp.h>
#endif

//#define T4_DO_TIME_PROFILING

#ifdef T4_DO_TIME_PROFILING
#define T4_ScopeProfiler(name) ::t4::ScopeProfiler scopeVar_##name(#name);
#else
#define T4_ScopeProfiler(X)
#endif

#ifdef _MSC_VER
#define T4_Pragma(X) __pragma(X)
#else
#define T4_Pragma(X) _Pragma(#X)
#endif

#if T4_USE_OMP
#define OMP_THREAD_ID omp_get_thread_num()
#define OMP_MAX_THREADS omp_get_max_threads()
#define parallel_for T4_Pragma(omp parallel for) for
#else
#define OMP_THREAD_ID 0
#define OMP_MAX_THREADS 1
#define parallel_for for
#endif


namespace t4
{
	typedef int64_t int64;

	class ScopeProfiler
	{
	public:
		ScopeProfiler(const char* name)
		{
			m_name = name;
			m_startTime = std::chrono::high_resolution_clock::now();
		};

		~ScopeProfiler()
		{
			printf("%-20s: %8dus\n", m_name, GetTime());
		};

		int GetTime() const
		{
			auto diffrence = std::chrono::high_resolution_clock::now() - m_startTime;
			unsigned long long diffrence_us = std::chrono::duration_cast<std::chrono::microseconds>(diffrence).count();
			return static_cast<int>(diffrence_us);
		}

	private:
		const char* m_name;
		std::chrono::high_resolution_clock::time_point m_startTime;
	};

	namespace memory
	{
		enum {
			PAGE_4K = 4096,
			BLOCK_SIZE = 128
		};

		inline void* aligned_malloc(size_t size, int alignment)
		{
#ifdef _MSC_VER
			return _aligned_malloc(size, alignment);
#elif __MINGW32__
			return __mingw_aligned_malloc(size, alignment);
#else
			void* ptr = nullptr;
			int result = posix_memalign(&ptr, alignment, size);
			return (result == 0) ? ptr : nullptr;
#endif
		}

		inline void aligned_free(void *p)
		{
#ifdef _MSC_VER
			_aligned_free(p);
#else
			free(p);
#endif
		}
	}

	// Templated class for remresenting n-dimentional tensor
	// template args:
	//     T - datatype. Should be any of: float, double, int, int64_t, int32_t, int16_t
	//     D - number of dimentions
	template<typename T, int D>
	class tensor
	{
		template<typename T2, int D2>
		friend class tensor;
	public:
		enum
		{
			ndim = D
		};

		// Creates a tensor of given shape and copies data from provided raw data pointer
		static tensor<T, D> New(const std::array<int64, D>& shape, T* data, bool create_copy = true)
		{
			tensor<T, D> t;
			t.m_shape = shape;
			t.m_offset = 0;
			if (create_copy)
			{
				t.m_ptr.reset(new T[(size_t)t.size()]);
				memcpy(t.ptr(), data, (size_t)t.size() * sizeof(T));
			}
			else
			{
				t.m_ptr.reset(data, [](T* p) {});
			}
			return t;
		}

		// Creates a tensor of given shape and copies data from provided raw data pointer
		static tensor<T, D> New(const std::array<int64, D>& shape, const T* data)
		{
			tensor<T, D> t;
			t.m_shape = shape;
			t.m_offset = 0;
			t.m_ptr.reset(new T[(size_t)t.size()]);
			memcpy(t.ptr(), data, (size_t)t.size() * sizeof(T));
			return t;
		}

		// Creates a tensor of given shape and takes provided shared pointer. 
		static tensor<T, D> New(const std::array<int64, D>& shape, std::shared_ptr<T> data, int64 offset = 0)
		{
			tensor<T, D> t;
			t.m_ptr = data;
			t.m_shape = shape;
			t.m_offset = offset;
			t.m_ptr = data;
			return t;
		}

		// Creates a tensor of given shape and allocates data for the new tensor.
		// If zero_initialize is true, will also zeroinitialize it.
		static tensor<T, D> New(const std::array<int64, D>& shape)
		{
			tensor<T, D> t;
			t.m_shape = shape;
			t.m_offset = 0;
			t.m_ptr.reset(new T[(size_t)t.size()]);
			return t;
		}

		// Creates a tensor of given shape and allocates data for the new tensor.
		// If zero_initialize is true, will also zeroinitialize it.
		static tensor<T, D> Zeros(const std::array<int64, D>& shape)
		{
			tensor<T, D> t = New(shape);
			memset(t.ptr(), 0, (size_t)t.size() * sizeof(T));
			return t;
		}

		// Returns a tensor filled with random numbers from a normal distribution with mean 0 and variance 1
		// Creates a tensor of given shape and allocates data for the new tensor.
		static tensor<T, D> RandN(const std::array<int64, D>& shape)
		{
			static std::random_device rd{};
			static std::mt19937 gen{ rd() };
			std::normal_distribution<double> distribution(T(0), T(1));
			tensor<T, D> t = New(shape);
			T* ptr = t.ptr();
			for (int64 i = 0, l = t.size(); i < l; ++i)
			{
				ptr[i] = distribution(gen);
			}
			return t;
		}

		// Creates a new tensor with the shape of this tensor.
		tensor<T, D> SameAs() const
		{
			return New(shape());
		}

		// Fills tensor with specified value.
		// Arguments:
		//     value - the value to fill with.
		void Fill(T value)
		{
			T* x = ptr();
			int64 i = 0;
			for (int64 l = size(); i < l - 4; i += 4)
			{
				x[i + 0] = value;
				x[i + 1] = value;
				x[i + 2] = value;
				x[i + 3] = value;
			}

			for (int64 l = size(); i < l; ++i)
				x[i] = value;
		}

		// Copies data from provided tensor
		void Assign(tensor<T, D> x)
		{
			T* dst = ptr();
			const T* src = x.ptr();
			memcpy(dst, src, size() * sizeof(T));
		}

		// Returns contiguous, one-dimentional reference tensor. Does not copy memory, 
		// e.g. all modification to the flattened tensor will also affect this tensor 
		tensor<T, 1> Contiguous() const
		{
			tensor<T, 1> t;
			t.m_ptr = m_ptr;
			t.m_offset = m_offset;
			t.m_shape[0] = size();
			return t;
		}

		// Returns new, flattened into a 2D matrix tensor.
		// Arguments:
		//     d - indicates up to which input dimensions (exclusive) should be flattened to the outer dimension of the output. 
		tensor<T, 2> Flatten(int d) const
		{
			int64 sizeA = 1;
			int64 sizeB = 1;
			for (int i = 0; i < d; ++i)
			{
				sizeA *= m_shape[i];
			}
			for (int i = d; i < ndim; ++i)
			{
				sizeB *= m_shape[i];
			}
			tensor<T, 2> t = tensor<T, 2>::New({ (int)sizeA, (int)sizeB });
			memcpy(t.ptr(), ptr(), (size_t)t.size() * sizeof(T));
			return t;
		}

		// Returns a reference sub tensor. Subtensor will have one axis less compared to this tensor.
		// Does not copy memory, e.g. all modification to the sub tensor will also affect this tensor 
		// Arguments:
		//     n - indicates the index of a slice of outermost axis from which the subtensor should be taken
		tensor<T, D - 1> Sub(int64 n) const
		{
			tensor<T, D - 1> t;
			t.m_ptr = m_ptr;
			for (int i = 1; i < ndim; ++i)
			{
				t.m_shape[i - 1] = m_shape[i];
			}
			t.m_offset = m_offset + n * t.size();
			return t;
		}

		// Returns a reference sub tensor. Subtensor will have two axis less compared to this tensor.
		// Does not copy memory, e.g. all modification to the sub tensor will also affect this tensor 
		// Arguments:
		//     n1 - indicates the index of a slice of outermost axis
		//     n2 - indicates the index of a slice of next axis
		tensor<T, D - 2> Sub(int64 n1, int64 n2) const
		{
			tensor<T, D - 2> t;
			t.m_ptr = m_ptr;
			for (int i = 2; i < ndim; ++i)
			{
				t.m_shape[i - 2] = m_shape[i];
			}
			t.m_offset = m_offset + n1 * t.size() * m_shape[1] + n2 * t.size();
			return t;
		}

		tensor<T, D + 1> expand()
		{
			std::array<int64, D + 1> new_shape;
			new_shape[0] = 1;
			for (int i = 0; i < D; ++i)
			{
				new_shape[i + 1] = m_shape[i];
			}
			return tensor<T, D + 1>::New(new_shape, m_ptr, m_offset);
		}

		// Returns a new tensor of the indices that would sort an array along the given axis.
		// Default axis is -1, which means the last axis.
		tensor<int64, D> Argsort(int axis = -1) const
		{
			assert(axis == -1 || axis < ndim);
			axis = (axis == -1) ? ndim - 1 : axis;

			tensor<int64, D> indixes = tensor<int64, D>::New(shape());
			int64 elementCount = size();
			int64 count = shape()[axis];
			int64 sortInstances = elementCount / count;
			int64 stride = 1;
			if (axis != -1)
			{
				for (int i = axis + 1; i < ndim; ++i)
				{
					stride *= m_shape[i];
				}
			}

			int64* indixesPtr = indixes.ptr();
			const T* dataPtr = ptr();

			int threads_n = OMP_MAX_THREADS;
			const size_t size_per_thr = ((count * sizeof(int64) + memory::PAGE_4K - 1) / memory::PAGE_4K) * memory::PAGE_4K;
			int64 *copy_buffers = (int64*)memory::aligned_malloc(threads_n * size_per_thr, memory::PAGE_4K);

			parallel_for(int i = 0; i < sortInstances; ++i)
			{
				int thread_id = OMP_THREAD_ID;
				int64 *copy_buff = copy_buffers + size_per_thr / sizeof(T) * thread_id;
				for (int j = 0; j < count; ++j)
				{
					copy_buff[j] = j;
				}
				const T* start = dataPtr + (i / stride) * count * stride + (i % stride);
				std::sort(copy_buff, copy_buff + count, [start, stride](int64 i1, int64 i2) {return start[i1 * stride] < start[i2 * stride]; });
				for (int j = 0; j < count; ++j)
				{
					indixesPtr[(i / stride) * count * stride + (i % stride) + j * stride] = copy_buff[j];
				}
			}
			memory::aligned_free(copy_buffers);
			return indixes;
		}

		// Returns a new tensor with element flipped along the given axis. 
		// Default axis is -1, which means the last axis.
		tensor<int64, D> Flip(int axis = -1) const
		{
			if (axis < 0)
			{
				axis += ndim;
			}
			assert(axis > 0 && axis < ndim);

			tensor<T, D> output = tensor<T, D>::New(shape());
			int64 elementCount = size();
			int64 count = shape()[axis];
			int64 flipInstances = elementCount / count;
			int64 stride = 1;
			if (axis != -1)
			{
				for (int i = axis + 1; i < ndim; ++i)
				{
					stride *= m_shape[i];
				}
			}

			int64* dst = output.ptr();
			const T* src = ptr();

			parallel_for(int i = 0; i < flipInstances / stride; ++i)
			{
				T* dstp = dst + i * count * stride;
				const T* srcp = src + i * count * stride;
				for (int j = 0; j < count; ++j)
				{
					memcpy(dstp + j * stride, srcp + (count - 1 - j) * stride, stride * sizeof(T));
				}

			}

			return output;
		}

		// Returns a raw pointer to the data
		T* ptr()
		{
			return (T*)(m_ptr.get() + m_offset);
		}

		// Returns a raw const pointer to the data
		const T* ptr() const
		{
			return (T*)(m_ptr.get() + m_offset);
		}

		// Returns smart pointer to the data
		std::shared_ptr<T> sptr() const
		{
			return m_ptr;
		}

		// Returns smart pointer to the data
		int64 GetOffset() const
		{
			return m_offset;
		}

		// Returns number of elements in the tensor. It is equal to the product of dimentions of all axis.
		int64 size() const
		{
			int64 size = 1;
			for (int i = 0; i < ndim; ++i)
			{
				size *= m_shape[i];
			}
			return size;
		}

		// Returns the shape of the tensor
		const std::array<int64, D>& shape() const
		{
			return m_shape;
		}

	private:
		std::shared_ptr<T> m_ptr;
		std::array<int64, D> m_shape;

		// Offset of the data of the tensor. The data the tensor starts from m_ptr.get() + m_offset.
		// It is used to create subtensors (which are effectivly slices) as references.
		int64 m_offset = 0;
	};

	template<typename T, int D>
	inline int width(const tensor<T, D>& t)
	{
		return t.shape()[D - 1];
	}

	template<typename T, int D>
	inline int height(const tensor<T, D>& t)
	{
		return t.shape()[D - 2];
	}

	template<typename T, int D>
	inline int channels(const tensor<T, D>& t)
	{
		return t.shape()[1];
	}

	template<typename T, int D>
	inline int number(const tensor<T, D>& t)
	{
		return t.shape()[0];
	}

	template<typename T, int D>
	inline tensor<int64, D> Argsort(const tensor<T, D>& x, int axis = -1)
	{
		return x.Argsort(axis);
	}

	typedef tensor<double, 4> tensor4d;
	typedef tensor<double, 3> tensor3d;
	typedef tensor<double, 2> tensor2d;
	typedef tensor<double, 1> tensor1d;
	typedef tensor<float, 4> tensor4f;
	typedef tensor<float, 3> tensor3f;
	typedef tensor<float, 2> tensor2f;
	typedef tensor<float, 1> tensor1f;
	typedef tensor<int64, 4> tensor4i;
	typedef tensor<int64, 3> tensor3i;
	typedef tensor<int64, 2> tensor2i;
	typedef tensor<int64, 1> tensor1i;
	typedef tensor<int64, 0> tensor0i;

	namespace data_loading
	{
		template<typename T>
		inline bool check_type(const std::string& str)
		{
			return false;
		}

		template<>
		inline bool check_type<float>(const std::string& str)
		{
			return str == "float";
		}

		template<>
		inline bool check_type<double>(const std::string& str)
		{
			return str == "doubl";
		}

		template<>
		inline bool check_type<int32_t>(const std::string& str)
		{
			return str == "int32";
		}

		template<>
		inline bool check_type<int16_t>(const std::string& str)
		{
			return str == "int16";
		}

		// Reinterpret cast of shared pointer
		template< class T, class U >
		inline std::shared_ptr<T> reinterpret_pointer_cast(const std::shared_ptr<U>& r)
		{
			auto p = reinterpret_cast<typename std::shared_ptr<T>::element_type*>(r.get());
			return std::shared_ptr<T>(r, p);
		}

		inline size_t get_size(const std::string& type)
		{
			if (type == "float")
			{
				return 4;
			}
			else if (type == "doubl")
			{
				return 6;
			}
			else if (type == "int32")
			{
				return 4;
			}
			else if (type == "int16")
			{
				return 2;
			}
			return 0;
		}
	}

	// Holds parameters of the network.
	// Contains a map that associates weight names with entries.
	// Entries have type, number of dimentions and shape. 
	// Those properties are checked when the load method is used to create new weight tensor.
	class model_dict
	{
		friend void save(model_dict md, const std::string& filename);
		friend model_dict compress(model_dict md, int compression);
		friend model_dict decompress(model_dict md, int compression);
	private:
		struct Entry
		{
			std::string type;
			int ndim;
			uint32_t shape[4];
			uint64_t size;
			uint64_t compressed_size;
			std::shared_ptr<uint8_t> ptr;
		};

		std::map<std::string, Entry> m_parameters;

	public:
		template<typename T>
		void load(tensor<T, 4>& t, const char* name, int n, int c, int h, int w)
		{
			Entry entry = m_parameters[name];
			assert(entry.shape[0] == n);
			assert(entry.shape[1] == c);
			assert(entry.shape[2] == h);
			assert(entry.shape[3] == w);
			assert(data_loading::check_type<T>(entry.type));
			assert(entry.compressed_size == 0);

			t = tensor<T, 4>::New({ n, c, h, w }, data_loading::reinterpret_pointer_cast<T>(entry.ptr));
		}

		template<typename T>
		void load(tensor<T, 3>& t, const char* name, int c, int h, int w)
		{
			Entry entry = m_parameters[name];
			assert(entry.shape[0] == c);
			assert(entry.shape[1] == h);
			assert(entry.shape[2] == w);
			assert(data_loading::check_type<T>(entry.type));
			assert(entry.compressed_size == 0);

			t = tensor<T, 3>::New({ w }, data_loading::reinterpret_pointer_cast<T>(entry.ptr));
		}

		template<typename T>
		void load(tensor<T, 2>& t, const char* name, int h, int w)
		{
			Entry entry = m_parameters[name];
			assert(entry.shape[0] == h);
			assert(entry.shape[1] == w);
			assert(data_loading::check_type<T>(entry.type));
			assert(entry.compressed_size == 0);

			t = tensor<T, 2>::New({ h, w }, data_loading::reinterpret_pointer_cast<T>(entry.ptr));
		}

		template<typename T>
		void load(tensor<T, 1>& t, const char* name, int w)
		{
			Entry entry = m_parameters[name];
			assert(entry.shape[0] == w);
			assert(data_loading::check_type<T>(entry.type));
			assert(entry.compressed_size == 0);

			t = tensor<T, 1>::New({ w }, data_loading::reinterpret_pointer_cast<T>(entry.ptr));
		}

		void add_parameter(const std::string& name, const std::string& type, int ndim, const uint32_t shape[4], uint64_t size, uint64_t compressed_size, std::shared_ptr<uint8_t> ptr)
		{
			m_parameters[name] = Entry({ type, ndim, { shape[0], shape[1], shape[2], shape[3] }, size, compressed_size, ptr });
		}
	};

	// Loads network parameters from file. Creates new model_dict.
	inline model_dict load(const std::string& filename)
	{
		T4_ScopeProfiler(loading_time);
#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4996)
#endif
		FILE* file = fopen(filename.c_str(), "rb");
		if (file == nullptr)
		{
			file = fopen(("../" + filename).c_str(), "rb");
		}

#ifdef _MSC_VER
#pragma warning ( pop )
#endif
		fseek(file, 0L, SEEK_END);
		size_t file_size = ftell(file);
		fseek(file, 0L, SEEK_SET);

		model_dict md;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
		while (true)
		{
			std::string weight_name;
			char buff;
			while (fread(&buff, 1, 1, file), buff != 0)
			{
				weight_name.push_back(buff);
			}
			char type[6];
			fread(&type, 1, 5, file);
			type[5] = 0;
			std::string type_str = type;
			uint8_t ndim = 0;
			fread(&ndim, 1, 1, file);

			uint32_t shape[4] = { 1, 1, 1, 1 };
			for (int i = 0; i < ndim; ++i)
			{
				fread(&shape[i], 1, 4, file);
			}

			std::shared_ptr<uint8_t> ptr;
			size_t param_size = data_loading::get_size(type) * shape[0] * shape[1] * shape[2] * shape[3];
			ptr.reset(new uint8_t[param_size]);
			uint64_t size = 0;
			fread(&size, sizeof(uint64_t), 1, file);
			uint64_t compressed_size = 0;
			fread(&compressed_size, sizeof(uint64_t), 1, file);
			assert(param_size == size);
			param_size = compressed_size == 0 ? param_size: compressed_size;
			fread(ptr.get(), param_size, 1, file);
			md.add_parameter(weight_name, type, ndim, shape, size, compressed_size, ptr);

			if (ftell(file) == file_size)
			{
				break;
			}
		}
#pragma GCC diagnostic pop
		fclose(file);

		return md;
	}

	inline void save(const t4::model_dict md, const std::string& filename)
	{
		FILE* file = fopen(filename.c_str(), "wb");

		for (const auto& entry: md.m_parameters)
		{
			fwrite(entry.first.c_str(), entry.first.size() + 1, 1, file);
			assert(entry.second.type.size() == 5);
			fwrite(entry.second.type.c_str(), 5, 1, file);
			fwrite(&entry.second.ndim, 1, 1, file);
			for (int i = 0; i < entry.second.ndim; ++i)
			{
				fwrite(&entry.second.shape[i], 1, 4, file);
			}
			auto shape = entry.second.shape;
			uint64_t param_size = t4::data_loading::get_size(entry.second.type) * shape[0] * shape[1] * shape[2] * shape[3];
			fwrite(&param_size, sizeof(uint64_t), 1, file);
			fwrite(&entry.second.compressed_size, sizeof(uint64_t), 1, file);
			param_size = entry.second.compressed_size == 0 ? param_size: entry.second.compressed_size;
			fwrite(entry.second.ptr.get(), param_size, 1, file);
		}

		fclose(file);
	}

	namespace details
	{
		template<typename T>
		inline void do_block(int LDA, int LDB, int LDC, int M, int N, int K, const T* __restrict A, const T* __restrict B, T* __restrict C, T* __restrict Bcopy)
		{
			int j, l, i;
			for (j = 0; j < N; ++j)
			{
				for (l = 0; l < K; ++l)
				{
					memcpy(Bcopy + l, B + j + l * LDB, sizeof(T));
				}
				for (i = 0; i < M; ++i)
				{
					const T* __restrict _A = A + i * LDA;
					float cij = C[j + i * LDC];
					for (l = 0; l < K; ++l)
					{
						cij += _A[l] * Bcopy[l];
					}
					C[j + i * LDC] = cij;
				}
			}
		}

		template<typename T>
		inline void do_block_nt(int LDA, int LDB, int LDC, int M, int N, int K, const T* __restrict A, const T* __restrict B, T* __restrict C)
		{
			int j, l, i;
			for (j = 0; j < N; ++j)
			{
				for (i = 0; i < M; ++i)
				{
					float cij = C[j + i * LDC];
					for (l = 0; l < K; ++l)
					{
						cij += (A + i * LDA)[l] * (B + j * LDB)[l];
					}
					memcpy(C + j + i * LDC, &cij, sizeof(T));
				}
			}
		}

		template<typename T1, typename T2>
		inline T2 min(T1 a, T2 b)
		{
			return a < b ? a : b;
		}

		// A: M x K
		// B: K x N
		// C: M x N
		template<typename T>
		inline void gemm_nn(int M, int N, int K, const T* A, int LDA, const T* B, int LDB, T* C, int LDC)
		{
#ifdef USE_MKL
			float alpha = 1.0f;
			float betta = 1.0f;
			cblas_sgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, N, M, K, alpha, B, LDB, A, LDA, betta, C, LDC);
			return;
#endif

			int threads_n = OMP_MAX_THREADS;
			const size_t elems_per_thr = K;
			const size_t size_per_thr = ((memory::BLOCK_SIZE * sizeof(T) + memory::PAGE_4K - 1) / memory::PAGE_4K) * memory::PAGE_4K;
			T *copy_buffers = (T*)memory::aligned_malloc(threads_n * size_per_thr, memory::PAGE_4K);

			parallel_for(int j = 0; j < N; j += memory::BLOCK_SIZE)
			{
				int thread_id = OMP_THREAD_ID;

				int _N = min(memory::BLOCK_SIZE, N - j);
				for (int i = 0; i < M; i += memory::BLOCK_SIZE)
				{
					int _M = min(memory::BLOCK_SIZE, M - i);
					for (int l = 0; l < K; l += memory::BLOCK_SIZE)
					{
						int _K = min(memory::BLOCK_SIZE, K - l);
						do_block(LDA, LDB, LDC, _M, _N, _K, A + l + i * LDA, B + j + l * LDB, C + j + i*LDC, copy_buffers + size_per_thr / sizeof(T) * thread_id);
					}
				}
			}
			memory::aligned_free(copy_buffers);
		}

		template<typename T>
		inline void gemm_nt(int M, int N, int K, const T* A, int LDA, const T* B, int LDB, T* C, int LDC)
		{
			parallel_for(int j = 0; j < N; j += memory::BLOCK_SIZE)
			{
				int _N = min(memory::BLOCK_SIZE, N - j);
				for (int i = 0; i < M; i += memory::BLOCK_SIZE)
				{
					int _M = min(memory::BLOCK_SIZE, M - i);
					for (int l = 0; l < K; l += memory::BLOCK_SIZE)
					{
						int _K = min(memory::BLOCK_SIZE, K - l);

						do_block_nt(LDA, LDB, LDC, _M, _N, _K, A + l + i * LDA, B + l + j * LDB, C + j + i*LDC);
					}
				}
			}
		}

		// Performs memory copy of elements of size sizeof(T) bytes with stride 
		// src_stride * sizeof(T) bytes from src buffer to dst buffer.
		// Is used for creating more generalized code, since when src_stride is 1
		// it will substitute template specialization with single call to memcpy
		template<int src_stride, typename T>
		struct memcpy_extended
		{
			static void strided(T* __restrict dst, const T* __restrict src, int count)
			{
				int x = 0;
				for (x = 0; x < count - 4; x += 4)
				{
					memcpy(dst + x + 0, src + (x + 0) * src_stride, sizeof(T));
					memcpy(dst + x + 1, src + (x + 1) * src_stride, sizeof(T));
					memcpy(dst + x + 2, src + (x + 2) * src_stride, sizeof(T));
					memcpy(dst + x + 3, src + (x + 3) * src_stride, sizeof(T));
				}
				for (; x < count; ++x)
				{
					memcpy(dst + x, src + x * src_stride, sizeof(T));
				}
			}
		};

		// Specialization of the above function for the case when src_stride is 1
		template<typename T>
		struct memcpy_extended<1, T>
		{
			static void strided(T* __restrict dst, const T* __restrict src, int count)
			{
				memcpy(dst, src, sizeof(T) * count);
			}
		};

		// padding non-zero.
		template<int kernel_h, int kernel_w, int stride_h, int stride_w, int pad_h, int pad_w, int dilation_h, int dilation_w, typename T>
		struct im2col_process_row
		{
			static void apply(T* __restrict dst, const T* __restrict src, int fh, int fw, int inputWidth, int inputHeight, int outputWidth, int outputHeight)
			{
				int start = (pad_w - fw * dilation_w + stride_w - 1) / stride_w;
				int end = (inputWidth + pad_w - fw * dilation_w + stride_w - 1) / stride_w;
				int start_clipped = std::max(start, 0);
				int end_clipped = std::min(end, outputWidth);

				for (int y = 0; y < outputHeight; ++y)
				{
					int input_y = y * stride_h + fh * dilation_h - pad_h;
					if (input_y >= 0 && input_y < inputHeight)
					{
						if (start > 0)
						{
							memset(dst + y * outputWidth, 0, sizeof(T)*(size_t)start);
						}

						memcpy_extended<stride_w, T>::strided(dst + y * outputWidth + start_clipped, src + input_y * inputWidth + start_clipped * stride_w + fw * dilation_w - pad_w, end_clipped - start_clipped);

						if (end < outputWidth)
						{
							memset(dst + y * outputWidth + end, 0, sizeof(T)*(size_t)(outputWidth - end));
						}
					}
					else
					{
						memset(dst + y * outputWidth, 0, sizeof(T)*(size_t)outputWidth);
					}
				}
			}
		};

		// padding zero.
		template<int kernel_h, int kernel_w, int stride_h, int stride_w, int dilation_h, int dilation_w, typename T>
		struct im2col_process_row<kernel_h, kernel_w, stride_h, stride_w, 0, 0, dilation_h, dilation_w, T>
		{
			static void apply(T* __restrict dst, const T* __restrict src, int fh, int fw, int inputWidth, int inputHeight, int outputWidth, int outputHeight)
			{
				for (int y = 0; y < outputHeight; ++y)
				{
					int input_y = y * stride_h + fh * dilation_h;
					memcpy_extended<stride_w, T>::strided(dst + y * outputWidth, src + input_y * inputWidth + fw * dilation_w, outputWidth);
				}
			}
		};

		template<int kernel_h, int kernel_w, int stride_h, int stride_w, int pad_h, int pad_w, int dilation_h, int dilation_w, typename T>
		inline void im2col(
			T* __restrict output,
			const T* __restrict input,
			int channels,
			int inputWidth,
			int inputHeight,
			int outputWidth,
			int outputHeight)
		{
			int64 channel_stride_in = (int64)inputHeight * inputWidth;
			int64 channel_stride_out = (int64)outputHeight * outputWidth;
			int column_size = channels * kernel_h * kernel_w;

			parallel_for(int row = 0; row < column_size; ++row)
			{
				int channel = row / kernel_h / kernel_w;
				int fh = (row / kernel_w) % kernel_h;
				int fw = row % kernel_w;

				T* __restrict dst = output + row * channel_stride_out;
				const T* __restrict src = input + channel * channel_stride_in;

				im2col_process_row<kernel_h, kernel_w, stride_h, stride_w, pad_h, pad_w, dilation_h, dilation_w, T>::apply(dst, src, fh, fw, inputWidth, inputHeight, outputWidth, outputHeight);
			}
		}
		
		template<int kernel_h, int kernel_w, int stride_h, int stride_w, int pad_h, int pad_w, int dilation_h, int dilation_w, typename T>
		inline void col2im(
			T* __restrict output,
			const T* __restrict input,
			int channels,
			int inputWidth,
			int inputHeight,
			int outputWidth,
			int outputHeight)
		{
			int64 channel_stride_in = (int64)inputHeight * inputWidth;
			int64 channel_stride_out = (int64)outputHeight * outputWidth;
			int column_size = channels * kernel_h * kernel_w;
			parallel_for(int row = 0; row < column_size; ++row)
			{
				int channel = row / kernel_h / kernel_w;
				int fh = (row / kernel_w) % kernel_h;
				int fw = row % kernel_w;

				int start = std::max((pad_w - fw * dilation_w + stride_w - 1) / stride_w, 0);
				int end = std::min((inputWidth + pad_w - fw * dilation_w + stride_w - 1) / stride_w, outputWidth);

				T* __restrict dst = output + channel * channel_stride_in;
				const T* __restrict src = input + row * channel_stride_out;

				for (int y = 0; y < outputHeight; ++y)
				{
					int input_y = y * stride_h + fh * dilation_h - pad_h;

					if (input_y >= 0 && input_y < inputHeight)
					{
						for (int x = start; x < end; ++x)
						{
							int input_x = x * stride_w + fw * dilation_w - pad_w;
							dst[input_y * inputWidth + input_x] += src[y * outputWidth + x];
						}
					}
				}
			}
		}
	}
	

	template<int kernel_h, int kernel_w, int stride_h, int stride_w, int pad_h, int pad_w, int dilation_h, int dilation_w, typename T>
	inline tensor<T, 4> Conv2d(
		tensor<T, 4> in
		, const tensor<T, 4> kernel
		, const tensor<T, 1> bias = tensor<T, 1>())
	{
		T4_ScopeProfiler(Conv2d);
		assert(channels(kernel) == channels(in));
		assert(kernel_h == height(kernel));
		assert(kernel_w == width(kernel));

		const int N = number(in);
		const int K = number(kernel);
		const int C = channels(kernel);
		const int Hin = height(in);
		const int Win = width(in);

		const int Hout = (Hin + 2 * pad_h - dilation_h * (kernel_h - 1) - 1) / stride_h + 1;
		const int Wout = (Win + 2 * pad_w - dilation_w * (kernel_w - 1) - 1) / stride_w + 1;

		T* __restrict columns = nullptr;
		columns = (T*)malloc(C * kernel_h * kernel_w * Hout * Wout * sizeof(T));

		details::im2col<kernel_h, kernel_w, stride_h, stride_w, pad_h, pad_w, dilation_h, dilation_w>(columns, in.ptr(), channels(in), Win, Hin, Wout, Hout);

		tensor<T, 4> out;

		if (bias.ptr() != nullptr)
		{
			out = tensor<T, 4>::New({ N, K, Hout, Wout });
			const T* pbias = bias.ptr();
			for (int n = 0; n < N; ++n)
			{
				parallel_for(int c = 0; c < K; ++c)
				{
					tensor<T, 2> t = out.Sub(n, c);
					t.Fill(pbias[c]);
				}
			}
		}
		else
		{
			out = tensor<T, 4>::Zeros({ N, K, Hout, Wout });
		}

		{
			T4_ScopeProfiler(Conv2d_gemm_nn);
			details::gemm_nn(K, Hout * Wout, kernel_h * kernel_w * C, kernel.ptr(), kernel_h * kernel_w * C, columns, Hout * Wout, out.ptr(), Hout * Wout);
		}

		free(columns);
		return out;
	}


	template<int kernel_h, int kernel_w, int stride_h, int stride_w, int pad_h, int pad_w, int dilation_h, int dilation_w, typename T>
	inline tensor<T, 4> ConvTranspose2d(
		tensor<T, 4> in
		, tensor<T, 4> kernel
		, tensor<T, 1> bias = tensor<T, 1>())
	{
		T4_ScopeProfiler(ConvTranspose2d);
		assert(number(kernel) == channels(in));
		assert(kernel_h == height(kernel));
		assert(kernel_w == width(kernel));

		const int N = number(in);
		const int K = channels(kernel);

		const int Hin = height(in);
		const int Win = width(in);

		const int Hout = (Hin - 1) * stride_h - 2 * pad_h + dilation_h * (kernel_h - 1) + 1;
		const int Wout = (Win - 1) * stride_w - 2 * pad_w + dilation_w * (kernel_w - 1) + 1;

		T* __restrict columns = (T*)malloc(sizeof(T) * K * kernel_h * kernel_w * Hin * Win);
		memset(columns, 0, K * kernel_h * kernel_w * Hin * Win * sizeof(T));

		{
			T4_ScopeProfiler(ConvTranspose2d_gemm_nn);
			int _M = K * kernel_h * kernel_w;
			int _K = number(kernel);
			T* __restrict AT = (T*)malloc(_M * _K * sizeof(T));
			const T* __restrict A = kernel.ptr();
			for (int i = 0; i < _M; ++i)
				for (int j = 0; j < _K; ++j)
					memcpy(AT + j + i * _K, A + i + j * _M, sizeof(T));

			details::gemm_nn(K * kernel_h * kernel_w, Hin * Win, number(kernel), AT, _K, in.ptr(), Hin * Win, columns, Hin * Win);
			free(AT);
		}

		tensor<T, 4> out;

		if (bias.ptr() != nullptr)
		{
			out = tensor<T, 4>::New({ N, K, Hout, Wout });
			T* pbias = bias.ptr();
			for (int n = 0; n < N; ++n)
			{
				for (int c = 0; c < K; ++c)
				{
					tensor<T, 2> t = out.Sub(n, c);
					t.Fill(pbias[c]);
				}
			}
		}
		else
		{
			out = tensor<T, 4>::Zeros({ N, K, Hout, Wout });
		}
		{
			T4_ScopeProfiler(ConvTranspose2d_col2im);
			details::col2im<kernel_h, kernel_w, stride_h, stride_w, pad_h, pad_w, dilation_h, dilation_w>(out.ptr(), columns, K, Wout, Hout, Win, Hin);
		}

		free(columns);

		return out;
	}

	template<typename T>
	inline tensor<T, 2> Linear(
		tensor<T, 2> in
		, tensor<T, 2> weight
		, tensor<T, 1> bias)
	{
		T4_ScopeProfiler(Linear);
		assert(width(in) == width(weight));
		assert(height(weight) == width(bias));
		const int N = number(in);
		const int Inputs = width(weight);
		const int Outputs = height(weight);

		tensor<T, 2> out;
		if (bias.ptr() != nullptr)
		{
			out = tensor<T, 2>::New({ N, Outputs });
			for (int n = 0; n < N; ++n)
			{
				out.Sub(n).Assign(bias);
			}
		}
		else
		{
			out = tensor<T, 2>::Zeros({ N, Outputs });
		}

		details::gemm_nt(N, Outputs, Inputs, in.ptr(), Inputs, weight.ptr(), Inputs, out.ptr(), Outputs);
		return out;
	}

	template<int kernel_h, int kernel_w, int stride_h, int stride_w, int pad_h, int pad_w, int dilation_h = 1, int dilation_w = 1, typename T>
	inline tensor<T, 4> MaxPool2d(tensor<T, 4> in)
	{
		T4_ScopeProfiler(MaxPool2d);
		const int N = number(in);
		const int C = channels(in);
		const int Hin = height(in);
		const int Win = width(in);

		const int Hout = (Hin + 2 * pad_h - dilation_h * (kernel_h - 1) - 1) / stride_h + 1;
		const int Wout = (Win + 2 * pad_w - dilation_w * (kernel_w - 1) - 1) / stride_w + 1;

		tensor<T, 4> out = tensor<T, 4>::New({ N, C, Hout, Wout });

		for (int n = 0; n < N; ++n)
		{
			parallel_for(int c = 0; c < C; ++c)
			{
				auto inSubtensor = in.Sub(n, c);
				const T* __restrict src = inSubtensor.ptr();

				auto outSubtensor = out.Sub(n, c);
				T* __restrict dst = outSubtensor.ptr();

				for (int i = 0; i < Hout; i++)
				{
					for (int j = 0; j < Wout; j++)
					{
						int start_h = i * stride_h - pad_h;
						int start_w = j * stride_w - pad_w;

						int end_h = std::min(start_h + (kernel_h - 1) * dilation_h + 1, Hin);
						int end_w = std::min(start_w + (kernel_w - 1) * dilation_w + 1, Win);

						start_h += ((std::max(-start_h, 0) + dilation_h - 1) / dilation_h) * dilation_h;
						start_w += ((std::max(-start_w, 0) + dilation_w - 1) / dilation_w) * dilation_w;

						T maxval = -std::numeric_limits<T>::max();
						for (int y = start_h; y < end_h; y += dilation_h)
						{
							for (int x = start_w; x < end_w; x += dilation_w)
							{
								T val = src[y * Win + x];
								if (val > maxval)
								{
									maxval = val;
								}
							}
						}
						dst[i * Wout + j] = maxval;
					}
				}
			}
		}
		return out;
	}


	template<int kernel_h, int kernel_w, int stride_h, int stride_w, int pad_h, int pad_w, int dilation_h = 1, int dilation_w = 1, typename T>
	inline tensor<T, 4> AveragePool2d(tensor<T, 4> in)
	{
		T4_ScopeProfiler(MaxPool2d);
		const int N = number(in);
		const int C = channels(in);
		const int Hin = height(in);
		const int Win = width(in);

		const int Hout = (Hin + 2 * pad_h - dilation_h * (kernel_h - 1) - 1) / stride_h + 1;
		const int Wout = (Win + 2 * pad_w - dilation_w * (kernel_w - 1) - 1) / stride_w + 1;

		tensor<T, 4> out = tensor<T, 4>::New({ N, C, Hout, Wout });

		for (int n = 0; n < N; ++n)
		{
			parallel_for(int c = 0; c < C; ++c)
			{
				auto inSubtensor = in.Sub(n, c);
				const T* __restrict src = inSubtensor.ptr();

				auto outSubtensor = out.Sub(n, c);
				T* __restrict dst = outSubtensor.ptr();

				for (int i = 0; i < Hout; i++)
				{
					for (int j = 0; j < Wout; j++)
					{
						int start_h = i * stride_h - pad_h;
						int start_w = j * stride_w - pad_w;

						int end_h = std::min(start_h + (kernel_h - 1) * dilation_h + 1, Hin);
						int end_w = std::min(start_w + (kernel_w - 1) * dilation_w + 1, Win);

						start_h += ((std::max(-start_h, 0) + dilation_h - 1) / dilation_h) * dilation_h;
						start_w += ((std::max(-start_w, 0) + dilation_w - 1) / dilation_w) * dilation_w;

						T sum = 0;
						for (int y = start_h; y < end_h; y += dilation_h)
						{
							for (int x = start_w; x < end_w; x += dilation_w)
							{
								sum += src[y * Win + x];
							}
						}
						dst[i * Wout + j] = sum / (end_h - start_h) / (end_w - start_w);
					}
				}
			}
		}
		return out;
	}

	template<typename T>
	inline tensor<T, 4> GlobalAveragePool2d(tensor<T, 4> in)
	{
		T4_ScopeProfiler(GlobalAveragePool2d)
		const int N = number(in);
		const int C = channels(in);
		const int Hin = height(in);
		const int Win = width(in);

		tensor<T, 4> out = tensor<T, 4>::New({ N, C, 1, 1 });

		for (int n = 0; n < N; ++n)
		{
			parallel_for(int c = 0; c < C; ++c)
			{
				auto inSubtensor = in.Sub(n, c);
				const T* __restrict src = inSubtensor.ptr();

				auto outSubtensor = out.Sub(n, c);
				T* __restrict dst = outSubtensor.ptr();

				T sum = T(0);
				for (int i = 0; i < Hin; i++)
				{
					int j = 0;
					const T* __restrict p =src + i * Win;
					for (j = 0; j < (Win-6)/6; j+=6)
					{
						sum += p[j + 0] + p[j + 1] + p[j + 2] + p[j + 3] + p[j + 4] + p[j + 5];
					}
					for (; j < Win; j++)
					{
						sum += p[j];
					}
				}
				*dst = sum / (Hin * Win);
			}
		}
		return out;
	}

	enum PaddingType
	{
		reflect,
		constant
	};

	// TODO: implement actual padding instead of just zero padiing
	template<PaddingType type, typename T>
	inline tensor<T, 4> Pad(
		tensor<T, 4> in
		, int p_x0_begin
		, int p_x1_begin
		, int p_x2_begin
		, int p_x3_begin
		, int p_x0_end
		, int p_x1_end
		, int p_x2_end
		, int p_x3_end)
	{
		T4_ScopeProfiler(Pad);
		tensor<T, 4> out = tensor<T, 4>::Zeros(
		{
			number(in) + p_x0_begin + p_x0_end,
			channels(in) + p_x1_begin + p_x1_end,
			height(in) + p_x2_begin + p_x2_end,
			width(in) + p_x3_begin + p_x3_end
		});

		for (int n = 0; n < number(in); ++n)
		{
			auto in_n = in.Sub(n);
			auto out_n = out.Sub(n + p_x0_begin);
			for (int c = 0; c < channels(in); ++c)
			{
				auto in_c = in_n.Sub(c);
				auto out_c = out_n.Sub(c + p_x1_begin);
				for (int h = 0; h < height(in); ++h)
				{
					auto in_h = in_c.Sub(h);
					auto out_h = out_c.Sub(h + p_x2_begin);

					memcpy(out_h.ptr() + p_x3_begin, in_h.ptr(), width(in_h) * sizeof(T));
				}
			}
		}

		return out;
	}

	template<size_t D>
	inline std::array<int64, D> BroadCastShape(const std::array<int64, D>& a, const std::array<int64, D>& b)
	{
		std::array<int64, D> result;
		for (int i = 0; i < D; ++i)
		{
			if (a[i] != b[i])
			{
				assert(a[i] == 1 || b[i] == 1);
			}
			result[i] = std::max(a[i], b[i]);
		}
		return result;
	}

	template<size_t D>
	inline std::array<int64, 4> ExpandShape(const std::array<int64, D>& x)
	{
		std::array<int64, 4> out = { 1, 1, 1, 1 };
		for (int i = 0; i < D; ++i)
		{
			out[D - i - 1] = x[D - i - 1];
		}
		return out;
	}

	template<size_t D>
	int64 ComputeWrappedIndex(int64 n, int64 c, int64 h, int64 w, const std::array<int64, D>& s);

	template<>
	inline int64 ComputeWrappedIndex<4>(int64 n, int64 c, int64 h, int64 w, const std::array<int64, 4>& s)
	{
		return (w % s[3]) + (h % s[2]) * s[3] + (c % s[1]) * s[3] * s[2] + (n % s[0]) * s[3] * s[2] * s[1];
	}

	template<>
	inline int64 ComputeWrappedIndex<3>(int64 n, int64 c, int64 h, int64 w, const std::array<int64, 3>& s)
	{
		return (w % s[2]) + (h % s[1]) * s[2] + (c % s[0]) * s[2] * s[1];
	}

	template<>
	inline int64 ComputeWrappedIndex<2>(int64 n, int64 c, int64 h, int64 w, const std::array<int64, 2>& s)
	{
		return (w % s[1]) + (h % s[0]) * s[1];
	}

	template<>
	inline int64 ComputeWrappedIndex<1>(int64 n, int64 c, int64 h, int64 w, const std::array<int64, 1>& s)
	{
		return (w % s[0]);
	}


#define POINT_WISE(OP) \
		auto out = in.SameAs(); \
		T*  __restrict dst = out.ptr(); \
		const T* __restrict src = in.ptr(); \
		int64 l = (int64)in.size(); \
		parallel_for (int64 i=0; i < l - 4; i += 4) \
		{ \
			{ T v = src[i + 0]; OP; dst[i + 0] = out; }\
			{ T v = src[i + 1]; OP; dst[i + 1] = out; }\
			{ T v = src[i + 2]; OP; dst[i + 2] = out; }\
			{ T v = src[i + 3]; OP; dst[i + 3] = out; }\
		} \
		for (int64 i=4 * ((l-4)/4); i < l; ++i) \
		{ \
			{ T v = src[i]; OP; dst[i] = out; }\
		}\
		return out;

#define POINT_INPLACE(OP) \
		T*  __restrict ptr = in.ptr(); \
		int64 l = (int64)in.size(); \
		parallel_for (int64 i=0; i < l - 4; i += 4) \
		{ \
			{ T v = ptr[i + 0]; OP; ptr[i + 0] = out; }\
			{ T v = ptr[i + 1]; OP; ptr[i + 1] = out; }\
			{ T v = ptr[i + 2]; OP; ptr[i + 2] = out; }\
			{ T v = ptr[i + 3]; OP; ptr[i + 3] = out; }\
		} \
		for (int64 i=4 * ((l-4)/4); i < l; ++i) \
		{ \
			{ T v = ptr[i]; OP; ptr[i] = out; }\
		}\
		return in;

#define POINT_WISE_BINARY(OP) \
		if (a.shape() == b.shape())\
		{\
			auto out = a.SameAs(); \
			T*  __restrict dst = out.ptr(); \
			const T* __restrict srcA = a.ptr(); \
			const T* __restrict srcB = b.ptr(); \
			int64 l =(int64)a.size(); \
			parallel_for (int64 i=0; i < l - 4; i += 4) \
			{ \
				{ T a = srcA[i + 0]; T b = srcB[i + 0]; OP; dst[i + 0] = out; }\
				{ T a = srcA[i + 1]; T b = srcB[i + 1]; OP; dst[i + 1] = out; }\
				{ T a = srcA[i + 2]; T b = srcB[i + 2]; OP; dst[i + 2] = out; }\
				{ T a = srcA[i + 3]; T b = srcB[i + 3]; OP; dst[i + 3] = out; }\
			} \
			for (int64 i=4 * ((l-4)/4); i < l; ++i) \
			{ \
				{ T a = srcA[i]; T b = srcB[i]; OP; dst[i] = out; }\
			}\
			return out;\
		} \
		else \
		{ \
			auto resultShape = BroadCastShape(a.shape(), b.shape()); \
			const auto s = ExpandShape(resultShape); \
			auto out = tensor<T, 4>::New(s); \
			T*  __restrict dst = out.ptr(); \
			const T* __restrict srcA = a.ptr(); \
			const T* __restrict srcB = b.ptr(); \
			auto sa = a.shape(); \
			auto sb = b.shape(); \
			for (int64 n = 0; n < s[0]; ++n) {\
				const T* __restrict srcAn = srcA + (n % sa[0]) * sa[3] * sa[2] * sa[1]; \
				const T* __restrict srcBn = srcB + (n % sb[0]) * sb[3] * sb[2] * sb[1]; \
				T* __restrict dstn = dst + n * s[3] * s[2] * s[1]; \
                parallel_for (int64 c = 0; c < s[1]; ++c) {\
					const T* __restrict srcAc = srcAn + (c % sa[1]) * sa[3] * sa[2]; \
					const T* __restrict srcBc = srcBn + (c % sb[1]) * sb[3] * sb[2]; \
					T* __restrict dstc = dstn + c * s[3] * s[2]; \
                    for (int64 h = 0; h < s[2]; ++h) {\
						const T* __restrict srcAh = srcAc + (h % sa[2]) * sa[3]; \
						const T* __restrict srcBh = srcBc + (h % sb[2]) * sb[3]; \
						T* __restrict dsth = dstc + h * s[3]; \
						if (sa[3]==1) {\
                            const T a = *srcAh; \
                            for (int64 w = 0; w < s[3]; ++w) \
                            { \
                                const T b = *(srcBh + w); \
                                OP; dsth[w] = out; \
                            }\
                        }else if (sb[3]==1){\
                            const T b = *srcBh; \
                            for (int64 w = 0; w < s[3]; ++w) \
                            { \
                                const T a = *(srcAh + w); \
                                OP; dsth[w] = out; \
                            }\
                        }else\
                        for (int64 w = 0; w < s[3]; ++w) \
                        { \
							const T a = *(srcAh + (w % sa[3])); \
							const T b = *(srcBh + (w % sb[3])); \
                            OP; dsth[w] = out; \
                        }\
                    }\
                }\
            }\
			auto out_tensor = tensor<T, D>::New(resultShape, out.sptr(), out.GetOffset());\
			return out_tensor;\
		}

	template<typename T, int D>
	inline tensor<T, D> LeakyRelu(const tensor<T, D>& in, float alpha)
	{
		T4_ScopeProfiler(LeakyRelu);
		POINT_WISE(
			T out = (v < 0) ? alpha * v : v;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> LeakyReluInplace(tensor<T, D>& in, float alpha)
	{
		T4_ScopeProfiler(LeakyReluInplace);
		POINT_INPLACE(
			T out = (v < 0) ? alpha * v : v;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> Relu(const tensor<T, D>& in)
	{
		T4_ScopeProfiler(Relu);
		POINT_WISE(
			T out = (v > 0) ? v : T(0);
		)
	}
	
	template<typename T, int D>
	inline tensor<T, D> ReluInplace(tensor<T, D>& in)
	{
		T4_ScopeProfiler(ReluInplace);
		POINT_INPLACE(
			T out = (v > 0) ? v : T(0);
		)
	}

	namespace details
	{
		template<typename T>
		T tanh_f(T x);

		template<typename T>
		T exp_f(T x);

		template<>
		inline float tanh_f<float>(float x)
		{
			return tanhf(x);
		}

		template<>
		inline double tanh_f<double>(double x)
		{
			return tanh(x);
		}

		template<>
		inline float exp_f<float>(float x)
		{
			return expf(x);
		}

		template<>
		inline double exp_f<double>(double x)
		{
			return exp(x);
		}
	}

	template<typename T, int D>
	inline tensor<T, D> Tanh(const tensor<T, D>& in)
	{
		POINT_WISE(
			T out = details::tanh_f(v);
		)
	}

	template<typename T, int D>
	inline tensor<T, D> Exp(const tensor<T, D>& in)
	{
		POINT_WISE(
			T out = details::exp_f(v);
		)
	}

	template<typename T, int D>
	inline tensor<T, D> Pow(const tensor<T, D>& in, T p)
	{
		T4_ScopeProfiler(Pow)
		POINT_WISE(
			T out = pow(v, p);
		)
	}

	template<typename T, int D>
	inline tensor<T, D> Neg(const tensor<T, D>& in)
	{
		POINT_WISE(
			T out = -v;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> Mul(const tensor<T, D>& in, T x)
	{
		POINT_WISE(
			T out = v * x;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> operator * (const tensor<T, D>& in, T x)
	{
		return Mul(in, x);
	}

	template<typename T, int D>
	inline tensor<T, D> Mul(const tensor<T, D>& a, const tensor<T, D>& b)
	{
		POINT_WISE_BINARY(
			T out = a * b;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> operator * (const tensor<T, D>& a, const tensor<T, D>& b)
	{
		return Mul(a, b);
	}

	template<typename T, int D>
	inline tensor<T, D> Add(const tensor<T, D>& in, T x)
	{
		POINT_WISE(
			T out = v + x;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> operator + (const tensor<T, D>& in, T x)
	{
		return Add(in, x);
	}

	template<typename T, int D>
	inline tensor<T, D> Add(const tensor<T, D>& a, const tensor<T, D>& b)
	{
		POINT_WISE_BINARY(
			T out = a + b;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> operator + (const tensor<T, D>& a, const tensor<T, D>& b)
	{
		return Add(a, b);
	}

	template<typename T, int D>
	inline tensor<T, D> Sub(const tensor<T, D>& a, const tensor<T, D>& b)
	{
		POINT_WISE_BINARY(
			T out = a - b;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> operator - (const tensor<T, D>& a, const tensor<T, D>& b)
	{
		return Sub(a, b);
	}

	template<typename T, int D>
	inline tensor<T, D> Div(const tensor<T, D>& a, const tensor<T, D>& b)
	{
		POINT_WISE_BINARY(
			T out = a / b;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> operator / (const tensor<T, D>& a, const tensor<T, D>& b)
	{
		return Div(a, b);
	}


	template<typename T, int D>
	inline tensor<T, D> Div(const tensor<T, D>& in, T x)
	{
		POINT_WISE(
			T out = v / x;
		)
	}

	template<typename T, int D>
	inline tensor<T, D> operator / (const tensor<T, D>& a, T x)
	{
		return Div(a, x);
	}

	template<int d, typename T, int D>
	inline tensor<T, 2> Flatten(const tensor<T, D>& in)
	{
		return in.Flatten(d);
	}

	template<typename T, int D>
	inline tensor<T, D> Dropout(const tensor<T, D>& in, float x)
	{
		return in;
	}

	template<int axis = -1, typename T, int D>
	inline tensor<T, D> Softmax(tensor<T, D> in)
	{
		T4_ScopeProfiler(Softmax);
		static_assert(axis == -1 || axis < D, "Wrong axis.");
		int _axis = (axis == -1) ? D - 1 : axis;

		tensor<T, D> output = Exp(in);
		int64 elementCount = in.size();
		int64 count = in.shape()[_axis];
		int64 sortInstances = elementCount / count;
		int64 stride = 1;
		if (axis != -1)
		{
			for (int i = _axis + 1; i < D; ++i)
			{
				stride *= in.shape()[i];
			}
		}

		T* dstPtr = output.ptr();

		parallel_for(int i = 0; i < sortInstances; ++i)
		{
			T* start = dstPtr + (i / stride) * count * stride + (i % stride);
			T sum = T(0);
			for (int j = 0; j < count; ++j)
			{
				sum += start[j * stride];
			}
			for (int j = 0; j < count; ++j)
			{
				start[j * stride] /= sum;
			}
		}

		return output;
	}

	template<typename T>
	inline tensor<T, 4> BatchNormalization(const tensor<T, 4> in,
		const tensor<T, 1> weight,
		const tensor<T, 1> bias,
		const tensor<T, 1> running_mean,
		const tensor<T, 1> running_var,
		float epsilon = 0.0f)
	{
		T4_ScopeProfiler(BatchNormalization);
		tensor<T, 4> out = in.SameAs();
		const T* __restrict bias_ptr = bias.ptr();
		const T* __restrict weight_ptr = weight.ptr();
		const T* __restrict running_mean_ptr = running_mean.ptr();
		const T* __restrict running_var_ptr = running_var.ptr();

		for (int n = 0; n < number(in); ++n)
		{
			parallel_for(int c = 0; c < channels(in); ++c)
			{
				tensor<T, 2> sub_in = in.Sub(n, c);
				tensor<T, 2> sub_out = out.Sub(n, c);

				const T* __restrict src = sub_in.ptr();
				T* __restrict dst = sub_out.ptr();

				T mul = weight_ptr[c];
				T add = bias_ptr[c];

				T mean = running_mean_ptr[c];
				T invstd = 1.0f / sqrtf(running_var_ptr[c] + epsilon);

				add -= mean * invstd * mul;
				mul *= invstd;

				int64 i = 0;
				for (int64 l = (int64)sub_in.size(); i < l - 4; i += 4)
				{
					dst[i + 0] = src[i + 0] * mul + add;
					dst[i + 1] = src[i + 1] * mul + add;
					dst[i + 2] = src[i + 2] * mul + add;
					dst[i + 3] = src[i + 3] * mul + add;
				}

				for (int64 l = (int64)sub_in.size(); i < l; ++i)
				{
					dst[i] = src[i] * mul + add;
				}
			}
		}
		return out;
	}

	template<typename T>
	inline tensor<T, 4> BatchNormalizationInplace(tensor<T, 4> in,
		const tensor<T, 1> weight,
		const tensor<T, 1> bias,
		const tensor<T, 1> running_mean,
		const tensor<T, 1> running_var,
		float epsilon = 0.0f)
	{
		const T* __restrict bias_ptr = bias.ptr();
		const T* __restrict weight_ptr = weight.ptr();
		const T* __restrict running_mean_ptr = running_mean.ptr();
		const T* __restrict running_var_ptr = running_var.ptr();

		for (int n = 0; n < number(in); ++n)
		{
			parallel_for(int c = 0; c < channels(in); ++c)
			{
				tensor<T, 2> sub = in.Sub(n, c);
				T* __restrict src = sub.ptr();
				T mul = weight_ptr[c];
				T add = bias_ptr[c];

				T mean = running_mean_ptr[c];
				T invstd = 1.0f / sqrtf(running_var_ptr[c] + epsilon);

				add -= mean * invstd * mul;
				mul *= invstd;

				int64 i = 0;
				for (int64 l = (int64)sub.size(); i < l - 4; i += 4)
				{
					src[i + 0] = src[i + 0] * mul + add;
					src[i + 1] = src[i + 1] * mul + add;
					src[i + 2] = src[i + 2] * mul + add;
					src[i + 3] = src[i + 3] * mul + add;
				}

				for (int64 l = (int64)sub.size(); i < l; ++i)
				{
					src[i] = src[i] * mul + add;
				}
			}
		}
		return in;
	}

	template<int axis = -1, typename T, int D>
	inline tensor<T, D> Concat(const tensor<T, D>& a, const tensor<T, D>& b)
	{
		T4_ScopeProfiler(Concat);
		static_assert(axis == -1 || axis < D, "Wrong axis.");
		int _axis = (axis == -1) ? D - 1 : axis;

		int64 elementCountA = a.size();
		int64 elementCountB = b.size();

		std::array<int64, D> result_shape;

		for (int i = 0; i < D; ++i)
		{
			if (i == axis)
			{
				result_shape[i] = a.shape()[i] + b.shape()[i];
			}
			else
			{
				assert(a.shape()[i] == b.shape()[i]);
				result_shape[i] = a.shape()[i];
			}
		}

		int64 blockCount = 1;

		for (int i = 0; i < axis; ++i)
		{
			blockCount *= a.shape()[i];
		}

		int64 strideA = elementCountA / blockCount;
		int64 strideB = elementCountB / blockCount;
		int64 strideR = strideA + strideB;

		tensor<T, D> out = tensor<T, D>::New(result_shape);

		T* __restrict dst = out.ptr();
		const T* __restrict srcA = a.ptr();
		const T* __restrict srcB = b.ptr();

		parallel_for(int64 i = 0; i < blockCount; ++i)
		{
			memcpy(dst + strideR * i, srcA + strideA * i, sizeof(T) * strideA);
			memcpy(dst + strideR * i + strideA, srcB + strideB * i, sizeof(T) * strideB);
		}

		return out;
	}

	template<typename T, int D>
	inline tensor1i Shape(tensor<T, D>& x)
	{
		return tensor1i::New({(int)x.shape().size()}, &x.shape()[0]);
	}

	template<typename T>
	inline tensor<T, 0> Constant(T x)
	{
		return tensor<T, 0>::New({}, &x);
	}

	template<typename T, int q, int r>
	tensor<T, q + r - 1> Gather(tensor<T, q> x, tensor<int64, r> idx, int axis=0)
	{
		std::array<int64, q + r - 1> result_shape;
		for (int i = 0; i < r - 1; ++i)
		{
			result_shape[i] = idx.shape()[i];
		}
		for (int i = 0; i < q; ++i)
		{
			if (i != axis)
			{
				result_shape[i + r - 1] = x.shape()[i];
			}
			else if (r > 0)
			{
				result_shape[i + r - 1] = idx.shape()[r-1];
			}
		}

		tensor<T, q + r - 1> result = tensor<T, q + r - 1>::New(result_shape);

		int64 blockSize = 1;
		int64 subblockCount = 1;
		int64 blockCount = 1;
		int64 gatherAxisDstSize = 1;
		int64 gatherAxisSrcSize = 1;

		for (int i = 0; i < (int64)idx.shape().size() - 1; ++i)
		{
			subblockCount *= idx.shape()[i];
		}

		for (int i = axis + 1; i < x.shape().size(); ++i)
		{
			blockSize *= x.shape()[i];
		}

		for (int i = 0; i < axis; ++i)
		{
			blockCount *= x.shape()[i];
		}

		if (idx.shape().size() > 0)
		{
			gatherAxisDstSize = idx.shape()[idx.shape().size() - 1];
		}
		gatherAxisSrcSize = x.shape()[axis];

		for (int64 i = 0; i < subblockCount; ++i)
		{
			for (int64 j = 0; j < blockCount; ++j)
			{
				for (int64 k = 0; k < gatherAxisDstSize; ++k)
				{
					int64 index = idx.ptr()[gatherAxisDstSize * i + k];
					memcpy(result.ptr() + blockSize * gatherAxisDstSize * blockCount * i + blockSize * gatherAxisDstSize * j + blockSize * k, x.ptr() + blockSize * gatherAxisSrcSize * j + blockSize * index, blockSize * sizeof(T));
				}
			}
		}

		return result;
	}

	template<typename T, int q>
	inline tensor<T, q - 1> Gather(tensor<T, q> x, int64 idx)
	{
		return Gather(x, tensor<int64, 0>::New({}, &idx));
	}

	template<int Dim, typename T, int D>
	inline tensor<T, D + 1> Unsqueeze(tensor<T, D> x)
	{
		std::array<int64, D + 1> result_shape;
		for (int i = 0; i < Dim; ++i)
		{
			result_shape[i] = x.shape()[i];
		}
		result_shape[Dim] = 1;
		for (int i = Dim; i < D; ++i)
		{
			result_shape[i + 1] = x.shape()[i];
		}

		return tensor<T, D + 1>::New(result_shape, x.sptr(), x.GetOffset());
	}

	template<int outDim, typename T, int D>
	inline tensor<T, outDim> Reshape(tensor<T, D> x, tensor<int64, 1> shape)
	{
		std::array<int64, outDim> result_shape;
		int unspecified_dim = -1;
		for (int i = 0; i < outDim; ++i)
		{
			result_shape[i] = shape.ptr()[i];
			if (result_shape[i] == -1)
			{
				assert(unspecified_dim == -1);
				unspecified_dim = i;
			}
		}
		int64 size = 1;
		for (int i = 0; i < result_shape.size(); ++i)
		{
			size *= result_shape[i];
		}
		if (size < 0)
		{
			size = -size;
			int64 d = x.size() / size;
			int64 rem = x.size() % size;
			assert(rem == 0);
			result_shape[unspecified_dim] = d;
		}

		auto out_tensor = tensor<T, outDim>::New(result_shape, x.sptr(), x.GetOffset());
		assert(out_tensor.size() == x.size());
		return out_tensor;
	}

	template<typename T, int D>
	inline void release(tensor<T, D>& x)
	{
		x = tensor<T, D>();
	}

	template<typename T, int D, typename ...Ts>
	inline void release(tensor<T, D>& x, Ts... args)
	{
		x = tensor<T, D>();
		release(args...);
	}

	namespace printing
	{
		template<typename T>
		inline bool isfinite(const T& x)
		{
			return true;
		}

		template<>
		inline bool isfinite(const float& x)
		{
			return ::isfinite(x);
		}

		template<>
		inline bool isfinite(const double& x)
		{
			return ::isfinite(x);
		}

		template<typename T, int D>
		inline int SetupFormat(std::ostream& stream, const tensor<T, D>& tensor)
		{
			constexpr int precision = 4;
			T max = -std::numeric_limits<T>::max();
			T min = std::numeric_limits<T>::max();
			bool has_fractional = false;
			const T* __restrict src = tensor.ptr();
			for (int64 i = 0, l = tensor.size(); i < l; ++i)
			{
				T x = abs(src[i]);
				if (isfinite(x))
				{
					max = x > max ? x : max;
					min = x < min ? x : min;
					has_fractional |= x != ceil(x);
				}
			}
			T emin = min != 0 ? floor(log10(min)) + 1 : 1;
			T emax = max != 0 ? floor(log10(max)) + 1 : 1;

			int width = 11;
			stream << std::scientific << std::setprecision(precision);

			if (has_fractional)
			{
				if (emax - emin < 5)
				{
					width = 6 + std::max((int)(emax), 1);
					stream << std::fixed << std::setprecision(precision);
				}
			}
			else
			{
				if (emax < 10)
				{
					width = (int)(emax + 1);
					stream.unsetf(std::ios_base::floatfield);
				}
			}
			return width;
		}

		inline void PrintIndent(std::ostream& stream, int indent)
		{
			for (int i = 0; i < indent; i++)
			{
				stream << " ";
			}
		}

		inline void PrintDots(std::ostream& stream, int indent, int level)
		{
			if (level > 1)
			{
				PrintIndent(stream, indent);
			}
			stream << "..., ";
			for (int64_t i = 0; i < level - 1; i++)
			{
				stream << "\n";
			}
		}

		inline void SkipEntries(std::ostream& stream, int& current, int total, int indent, int level)
		{
			if (total > 6 && current == 3)
			{
				PrintDots(stream, indent, level);
				current = total - 3;
			}
		}

		template<int level>
		class Printer
		{
		public:
			template<typename T>
			static void Print(std::ostream& output, const T* data, int indent, int width, const int64* shape)
			{
				output << "[";
				size_t stride = 1;
				for (int i = 1; i < level; ++i)
				{
					stride *= shape[i];
				}
				bool nextline = false;
				for (int i = 0; i < *shape; ++i)
				{
					SkipEntries(output, i, *shape, indent + 1, level);
					if (nextline && level > 1)
					{
						PrintIndent(output, indent + 1);
					}
					nextline = false;

					Printer<level - 1>::template Print<T>(output, data + stride * i, indent + 1, width, shape + 1);

					if (i != *shape - 1)
					{
						output << ",";
						for (int l = 1; l < level; ++l)
						{
							output << "\n";
						}
						nextline = true;
					}
				}
				output << "]";
			}
		};

		template<>
		class Printer<1>
		{
		public:
			template<typename T>
			static void Print(std::ostream& output, const T* data, int indent, int width, const int64* shape)
			{
				output << "[";
				for (int i = 0; i < *shape; ++i)
				{
					SkipEntries(output, i, *shape, indent + 1, 1);
					output << std::setw(width) << data[i];
					if (i != *shape - 1)
					{
						output << ", ";
					}
				}
				output << "]";
			}
		};

		template<typename T, int D>
		inline void PrintTensor(std::ostream& stream, const tensor<T, D>& tensor)
		{
			int width = SetupFormat(stream, tensor);

			stream << "tensor(";
			int indent = 7;

			if (tensor.size() != 0)
			{
				Printer<D>::template Print<T>(stream, tensor.ptr(), indent, width, tensor.shape().data());
			}

			stream << ")\n";
		}
	}

	template<typename T, int D>
	inline std::ostream& operator << (std::ostream& output, const tensor<T, D>& t)
	{
		printing::PrintTensor(output, t);
		return output;
	}
}

#ifdef T4_ScopeProfiler
#undef T4_ScopeProfiler
#endif
#define T4_ScopeProfiler(name) ::t4::ScopeProfiler scopeVar_##name(#name);
