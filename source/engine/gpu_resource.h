#pragma once
#include "render.h"

#include <memory>

class gpu_buffer
{
public:
	explicit gpu_buffer(const yar_buffer_desc* desc)
	{
		yar_buffer* tmp;
		add_buffer(const_cast<yar_buffer_desc*>(desc), &tmp);
		buffer.reset(tmp);
	}

	yar_buffer* get()
	{
		return buffer.get();
	}

private:
	struct gpu_buffer_deleter
	{
		void operator()(yar_buffer* b)
		{
			remove_buffer(b);
		}
	};

	std::unique_ptr<yar_buffer, gpu_buffer_deleter> buffer;
};