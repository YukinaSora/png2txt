export module Module;

// zlib for decompression
#include <zlib/zlib.h>

#include "utils.h"

import std;

export class PNG;

using std::ifstream;
using std::ofstream;
using std::cout;
using std::cerr;
using std::endl;
using std::hex;
using std::dec;
using std::setw;
using std::setfill;
using std::println;
using std::min;
using std::function;
using std::map;
using std::string;
using std::vector;
using std::array;
using std::shared_ptr;

typedef unsigned char byte;
typedef unsigned int uint;
typedef vector<byte> byte_array;
typedef array<byte, 3> rgb_t;
typedef array<byte, 4> rgba_t;
typedef vector<rgba_t> color_array;
typedef array<size_t, 2> pos_t;

class PNG
{
public:
	PNG(const char* filename, const char* output_path)
		: m_output_path{ output_path }, m_img { }, m_buffer{ }
		, m_size { 0 }, m_width{ 0 }, m_height{ 0 }
		, m_bit_depth{ 0 }, m_color_type{ 0 }
		, m_compression_method{ 0 }, m_filter_method{ 0 }
		, m_interlace_method{ 0 }, m_pixel_size{ 0 }
		, m_pos{ 0 }, m_read_size{ 0 }
		, m_chunk_length{ 0 }, m_chunk_type { }, m_data { }
		, m_compressed_data{ }, m_decompressed_data{ }
		, m_color_data{ }
		, m_output{ }
	{
		// binary: read file as binary instead of text
		m_img = ifstream{ filename, std::ios::binary };
		if (!m_img)
		{
			cerr << "Error: Could not open file " << filename << endl;
			return;
		}

		m_buffer.resize(buffer_size, 0);

		m_img.seekg(0, std::ios::end);
		m_size = m_img.tellg();
		m_img.seekg(0, std::ios::beg);
	}

	PNG(const string filename, const string output_path)
		: PNG(filename.c_str(), output_path.c_str())
	{ }

	void convert()
	{
		cout << "File size: " << m_size << endl;

		read();

		output();
	}

private:
	void read()
	{
		read_header();
		read_chunks();

		inflate_IDAT();

		// parse data based on color type
		auto parser = color_type_parsers.at(m_color_type);
		(this->*parser)();
	}

	void output()
	{
		// get parent directory of output file
		string dir = m_output_path;
		std::filesystem::create_directory(dir);

		string out_txt = dir + "\\output.txt";
		string out_html = dir + "\\output.html";

		ofstream out(out_txt);
		ofstream html(out_html);

		html << "<!DOCTYPE html>\n"
				"<html class='scrollable' style='background-color: black; '>\n"
				"	<head>\n"
				"		<title>PNG to TXT</title>\n"
				"	</head>\n"
				"	<body>\n"
				"		<pre style=\"line-height:8px; font-size: 6px\">\n";

		size_t i = 0;
		for (size_t y = 0; y < m_height; ++y)
		{
			for (size_t x = 0; x < m_width; ++x)
			{
				out << m_output[i] << m_output[i];
				html << string("<span"
								" style = 'color: "
								) 
								+ rgba_to_color(m_color_data[i])
								+ ";font-size-adjust:1;'>"
								//+ ";letter-spacing: 2px;'>"
					 << m_output[i] << m_output[i]
					 //<< "■"
					 << string("</span>");
				i++;
			}
			out << endl;
			html << "<br>";
			//cout << endl;
		}

		html << "		</pre>\n"
				"	</body>\n"
				"   <style>\n"
				"	   .scrollable::-webkit-scrollbar { display: none; }\n"
				"   </style>\n"
				"</html>\n";

		out.close();
		html.close();
		
		cout << "Output(" << dec << m_output.size() << " bytes) written to " << out_html << endl;

		return;
	}

	void read_for(size_t size = 0, bool clear_data = true)
	{
		if (clear_data)
		{
			m_data.clear();
		}

		if (m_pos + size > m_size)
		{
			size = m_size - m_pos;
		}

		if (size > buffer_size)
		{
			cout << "Error: Buffer overflow: " << size << ", max size: " << buffer_size << endl;
			cout << "Trying to read by steps..." << endl;

			size_t read = 0;
			while (read < size)
			{
				size_t step = min(buffer_size, size - read);
				read_for(step, false);
				read += step;
				m_read_size = read;
			}

			return;
		}

		m_read_size = size;
		//cout << "Read size: " << dec << m_read_size << ", pos: " << m_pos << ", size: " << size << ", file size: " << m_size << endl;
		m_img.read(reinterpret_cast<char*>(m_buffer.data()), m_read_size);
		m_data.insert(m_data.end(), m_buffer.begin(), m_buffer.end());
		m_pos += m_read_size;
	}

	void read_header()
	{
		read_for(header.size());
		cout << "Header: ";
		print_data();

		if (!compare_data_with(header))
		{
			cerr << "Error: Invalid PNG header." << endl;
			cerr << "Expected: ";
			print_data(header);
			return;
		}
	}

	void read_chunks()
	{
		m_chunk_type = byte_array(4, 0);

		while (m_pos < m_size) {
			m_data.clear();
			read_chunk();
			//cout << endl;
		}
	}

	void read_chunk()
	{
		// read length
		read_for(4);

		m_chunk_length = data_to_num();
		//cout << "Chunk length: " << dec << m_chunk_length << endl;


		// read chunk type
		read_for(4);

		visit_data([&](byte b, size_t i) {
			m_chunk_type[i] = b;
		});
		cout << "Reading ";
		print_string(m_chunk_type);
		cout << " Chunk(" << dec << m_chunk_length << " bytes)";
		cout << endl;


		// read chunk data
		// select reader based on chunk type
		m_data.resize(m_chunk_length);
		auto reader = chunk_readers.at(m_chunk_type);
		(this->*reader)();

		// read CRC
		read_for(4);
		//cout << "CRC: ";
		//print_data();
	}

	// do nothing but read chunk data
	void read_chunk_null()
	{
		read_for(m_chunk_length);
	}

	void read_chunk_IHDR()
	{
		//cout << "Reading IHDR chunk..." << endl;

		// read width and height
		read_for(4);
		m_width = data_to_num();

		read_for(4);
		m_height = data_to_num();

		m_color_data.resize(m_width * m_height);
		m_output.resize(m_width * m_height);

		// read bit depth, color type, compression method, filter method, interlace method
		read_for(5);
		m_bit_depth = m_buffer[0];
		m_color_type = m_buffer[1];
		m_compression_method = m_buffer[2];
		m_filter_method = m_buffer[3];
		m_interlace_method = m_buffer[4];

		if (m_color_type == 6) {
			m_pixel_size = 4;
		}
		else if (m_color_type == 2) {
			m_pixel_size = 3;
		}
		else {
			cerr << "Error: Unsupported color type: " << m_color_type << endl;
			return;
		}

		cout << "Width: " << dec << m_width << endl;
		cout << "Height: " << m_height << endl;
		cout << "Bit depth: " << m_bit_depth << endl;
		cout << "Color type: " << m_color_type << endl;
		cout << "Compression method: " << m_compression_method << endl;
		cout << "Filter method: " << m_filter_method << endl;
		cout << "Interlace method: " << m_interlace_method << endl;
	}

	void read_chunk_IDAT()
	{
		//cout << "Reading IDAT chunk..." << endl;

		read_for(m_chunk_length);
		//cout << "IDAT data(" << dec << m_chunk_length << " bytes): ";
		//print_data();

		m_compressed_data.insert(m_compressed_data.end(), 
								 m_data.begin(), 
								 m_data.begin() + m_chunk_length);
	}

	void read_chunk_PLET()
	{
		//cout << "Reading PLTE chunk..." << endl;

		read_for(m_chunk_length);
		print_data();
	}

	void read_chunk_pHYs()
	{
		//cout << "Reading pHYs chunk..." << endl;

		read_for(m_chunk_length);
		print_data();
	}

	void read_chunk_IEND()
	{
		//cout << "Reading IEND chunk..." << endl;
	}

	// use zlib to decompress IDAT chunk ( inflate compressed data )
	void inflate_IDAT()
	{
		// decompress deflate data
		cout << "Decompressing IDAT chunk..." << endl;

		// init zlib stream
		z_stream stream;
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;
		stream.avail_in = static_cast<uint>(m_compressed_data.size());
		stream.next_in = m_compressed_data.data();

		if (inflateInit(&stream) != Z_OK)
		{
			throw std::runtime_error("Failed to initialize zlib stream.");
		}

		// decompress data
		byte_array output;
		byte buffer[buffer_size];
		int ret;
		size_t i = 0;

		do {
			if (stream.avail_in == 0)
			{
				stream.avail_in = static_cast<uint>(m_read_size);
				stream.next_in = m_compressed_data.data() + i * buffer_size;
			}

			stream.avail_out = sizeof(buffer);
			stream.next_out = buffer;
			ret = inflate(&stream, Z_NO_FLUSH);

			if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
				inflateEnd(&stream);
				throw std::runtime_error("inflate failed");
			}

			output.insert(output.end(), buffer, buffer + sizeof(buffer) - stream.avail_out);

			i++;
		} 
		while (ret != Z_STREAM_END);

		inflateEnd(&stream);

		m_decompressed_data = output;

		// 计算预期的解压后数据长度
		//size_t bytes_per_line = m_width * m_pixel_size + 1; // 每行：1字节过滤类型 + 像素数据
		//size_t expected_size = m_height * bytes_per_line;

		//// 验证实际解压长度
		//if (m_decompressed_data.size() != expected_size) {
		//	cerr << "Error: Decompressed data size mismatch. Expected "
		//		<< expected_size << ", got " << m_decompressed_data.size() << endl;
		//	throw std::runtime_error("Decompressed data size invalid");
		//}

		//cout << "Decompressed data(" << dec << m_decompressed_data.size() << " bytes): ";
		//print_data(m_decompressed_data);
	}

	// parse rgba(type 6) inflated data to greyscale image
	void parse_color()
	{
		//cout << "Data size: " << dec << m_decompressed_data.size() << endl;
		for (size_t y = 0; y < m_height; ++y)
		{
			// + 1: line starts with filter method, then pixels
			size_t line_start = y * (m_width * m_pixel_size + 1);

			// filter method
			byte filter_method = m_decompressed_data[line_start + 0];
			//cout << "Filter method on (" << y << "): " << (uint)filter_method << endl;

			auto parser = rgba_filter_parsers.at(filter_method);

			for (size_t x = 0; x < m_width; ++x)
			{
				size_t pixel_start = line_start + 1 + x * m_pixel_size;
				rgba_t rgba = (this->*parser)({ x, y });
				byte grey = rgba_to_grey(rgba);

				m_color_data[x + y * m_width] = rgba;
				m_output[x + y * m_width] = grey;

				m_decompressed_data[pixel_start + 0] = rgba[0];
				m_decompressed_data[pixel_start + 1] = rgba[1];
				m_decompressed_data[pixel_start + 2] = rgba[2];
				m_decompressed_data[pixel_start + 3] = rgba[3];
			}

			if (y % 16 == 0 || y == m_height) {
				//cout << "Progress: " << dec << (float)(y + 1) / (float)m_height << endl;
			}
		}
	}

	byte rgb_to_grey(rgb_t rgb)
	{
		double grey = ( static_cast<float>(rgb[0]) * 0.299f
				      + static_cast<float>(rgb[1]) * 0.587f
				      + static_cast<float>(rgb[2]) * 0.114f);

		size_t index = (size_t)(grey / 255.0 * (greyscale.size() - 1));
		return greyscale[index];
	}

	byte rgba_to_grey(rgba_t rgba)
	{
		double grey = (  static_cast<float>(rgba[0])  * 0.299f
					  +	 static_cast<float>(rgba[1])  * 0.587f
					  +	 static_cast<float>(rgba[2])  * 0.114f)
					  * (static_cast<float>(rgba[3])) / 255
			;

		size_t index = (size_t)(grey / 255.0 * (greyscale.size() - 1));
		return greyscale[index];
	}

	string rgba_to_color(rgba_t rgba)
	{
		std::ostringstream o;
		for (size_t i = 0; i < 3; i++) {
			o << hex << setw(2) << setfill('0') << static_cast<int>(rgba[i]);
		}
		string color = string("#") + o.str();
		return color;
	}

	rgb_t rgb_on(pos_t pos)
	{
		size_t line_start  = pos[1] * (m_width * m_pixel_size + 1);
		size_t pixel_start = line_start + 1 + pos[0] * m_pixel_size;

		byte r = m_decompressed_data[pixel_start + 0];
		byte g = m_decompressed_data[pixel_start + 1];
		byte b = m_decompressed_data[pixel_start + 2];

		return { r, g, b };
	}

	rgba_t rgba_on(pos_t pos)
	{
		size_t line_start  = pos[1] * (m_width * m_pixel_size + 1);
		size_t pixel_start = line_start + 1 + pos[0] * m_pixel_size;

		byte r = m_decompressed_data[pixel_start + 0];
		byte g = m_decompressed_data[pixel_start + 1];
		byte b = m_decompressed_data[pixel_start + 2];
		byte a = m_decompressed_data[pixel_start + 3];

		return { r, g, b, a };
	}

	rgb_t filter_rgb_0(pos_t pos)
	{
		// do nothing
		return rgb_on(pos);
	}

	rgba_t filter_rgba_0(pos_t pos)
	{
		// do nothing
		return rgba_on(pos);
	}

	// sub filter: subtract pixel value from left
	rgb_t filter_rgb_1(pos_t pos)
	{
		rgb_t base = rgb_on(pos);

		if (pos[0] == 0) {
			return base;
		}

		rgb_t left = rgb_on({ pos[0] - 1, pos[1] });

		base[0] = ((uint)base[0] + (uint)left[0]) % 256;
		base[1] = ((uint)base[1] + (uint)left[1]) % 256;
		base[2] = ((uint)base[2] + (uint)left[2]) % 256;

		return base;
	}

	rgba_t filter_rgba_1(pos_t pos)
	{
		rgba_t base = rgba_on(pos);
		rgba_t left;

		if (pos[0] == 0) {
			left = rgba_t{ 0, 0, 0, 0 };
		}
		else {
			left = rgba_on({ pos[0] - 1, pos[1] });
		}

		base[0] = ((uint)base[0] + (uint)left[0]) % 256;
		base[1] = ((uint)base[1] + (uint)left[1]) % 256;
		base[2] = ((uint)base[2] + (uint)left[2]) % 256;
		base[3] = ((uint)base[3] + (uint)left[3]) % 256;

		return base;
	}

	rgba_t filter_rgba_2(pos_t pos)
	{
		rgba_t base = rgba_on(pos);

		rgba_t top;

		if (pos[1] == 0) {
			top = rgba_t{ 0, 0, 0, 0 };
		}
		else {
			top = rgba_on({ pos[0], pos[1] - 1 });
		}

		base[0] = ((uint)base[0] + (uint)top[0]) % 256;
		base[1] = ((uint)base[1] + (uint)top[1]) % 256;
		base[2] = ((uint)base[2] + (uint)top[2]) % 256;
		base[3] = ((uint)base[3] + (uint)top[3]) % 256;

		return base;
	}

	rgba_t filter_rgba_3(pos_t pos)
	{
		rgba_t base = rgba_on(pos);

		rgba_t left = (pos[0] > 0) ? rgba_on({ pos[0] - 1, pos[1] }) : rgba_t{ 0,0,0,0 };
		rgba_t top  = (pos[1] > 0) ? rgba_on({ pos[0], pos[1] - 1 }) : rgba_t{ 0,0,0,0 };

		for (int i = 0; i < 4; ++i) {
			int predicted = static_cast<int>(std::floor((left[i] + top[i]) / 2.0));
			base[i] = static_cast<byte>((base[i] + predicted) % 256);
		}

		return base;
	}

	// paeth filter
	rgb_t filter_rgb_4(pos_t pos)
	{
		auto base = rgb_on(pos);

		// pixels
		rgb_t a, b, c, p;
		size_t x_left, y_top;

		// get left pixel a
		if (pos[0] == 0) {
			x_left = 0;
			a = rgb_t{ 0, 0, 0 };
		}
		else {
			x_left = pos[0] - 1;
			a = rgb_on({ pos[0] - 1, pos[1] });
		}

		// get top pixel b
		if (pos[1] == 0) 
		{
			y_top = 0;
			b = rgb_t{ 0, 0, 0 };
		}
		else {
			y_top = pos[1] - 1;
			b = rgb_on({ pos[0], pos[1] - 1 });
		}

		// get top-left pixel c
		c = rgb_on({ x_left, y_top });

		for (size_t i = 0; i < 3; i++) {
			p[i] = a[i] + b[i] - c[i];

			byte pa = abs((int)p[i] - (int)a[i]);
			byte pb = abs((int)p[i] - (int)b[i]);
			byte pc = abs((int)p[i] - (int)c[i]);

			byte pr = min(min(pa, pb), pc);

			base[i] = (base[i] + pr) % 256;
		}

		return base;
	}

	rgba_t filter_rgba_4(pos_t pos)
	{
		rgba_t base = rgba_on(pos);

		// pixels
		rgba_t a, b, c;

		// get left pixel a
		if (pos[0] == 0) {
			a = rgba_t{ 0, 0, 0, 0 };
		}
		else {
			a = rgba_on({ pos[0] - 1, pos[1] });
		}

		// get top pixel b
		if (pos[1] == 0) {
			b = rgba_t{ 0, 0, 0, 0 };
		}
		else {
			b = rgba_on({ pos[0], pos[1] - 1 });
		}

		// get top-left pixel c
		if (pos[0] == 0 || pos[1] == 0) {
			c = rgba_t{ 0, 0, 0, 0 };
		}
		else {
			c = rgba_on({ pos[0] - 1, pos[1] - 1 });
		}

		for (size_t i = 0; i < 4; i++) {
			int p = (int)a[i] + (int)b[i] - (int)c[i];

			int pa = abs(p - (int)a[i]);
			int pb = abs(p - (int)b[i]);
			int pc = abs(p - (int)c[i]);

			byte pr;
			if (pa <= pb && pa <= pc) {
				pr = a[i];
			}
			else if (pb <= pc) {
				pr = b[i];
			}
			else {
				pr = c[i];
			}


			base[i] = (base[i] + pr) % 256;
		}

		return base;
	}
	
	size_t data_to_num()
	{
		size_t num = 0;
		visit_data([&](byte b, size_t _) {
			num = (num << 8) | b;
		});
		return num;
	}

	bool compare_data_with(const byte_array content)
	{
		try {
			visit_data([&](byte b, size_t i) {
				if (b != content[i])
				{
					throw i;
				}
				});
			return true;
		}
		catch (size_t i)
		{
			cout << "Error: Buffer mismatch at position (" << hex << i << ")."
				<< " Expected: " << hex << (uint)content[i]
				<< ", Got: " << hex << (uint)(byte)m_data[i] << endl;

			cout << "Buffer: ";
			print_data();

			cout << "Expected: ";

			print_data(content);
			return false;
		}
	}

	void visit_data(function<void(byte, size_t)> callback) const
	{
		for (size_t i = 0; i < m_read_size; ++i)
		{
			callback((byte)m_data[i], i);
		}
	}

	void print_data(const byte_array data = {}) const
	{
		size_t size = data.size();
		if (!data.size())
		{
			*const_cast<byte_array*>(&data) = m_data;
			size = m_read_size;
		}

		for (size_t i = 0; i < size; ++i)
		{
			cout << hex << setw(2) << setfill('0') << (uint)(byte)data[i] << " \0"[i == size - 1];
		}
		cout << endl;
	}

	void print_string(const byte_array data) const
	{
		for (size_t i = 0; i < data.size(); ++i)
		{
			cout << (char)data[i];
		}
	}

	int abs(int a)
	{
		return a < 0 ? -a : a;
	}

	int round_up(float a)
	{
		return static_cast<int>(a + .5f);
	}

public:
	constexpr static size_t buffer_size = 1 << 10 << 7; // 128KB

private:
	const string greyscale = " .:-=+*#%@";

	const byte_array header = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

	const byte_array IHDR = { 0x49, 0x48, 0x44, 0x52 };
	const byte_array IDAT = { 0x49, 0x44, 0x41, 0x54 };
	const byte_array PLTE = { 0x50, 0x4C, 0x54, 0x45 };
	const byte_array pHYs = { 0x70, 0x48, 0x59, 0x73 };
	const byte_array iTXt = { 0x69, 0x54, 0x58, 0x74 };
	const byte_array tEXt = { 0x74, 0x45, 0x58, 0x74 };
	const byte_array IEND = { 0x49, 0x45, 0x4E, 0x44 };

	const map<const byte_array, void(PNG::*)()> chunk_readers = {
		{ IHDR, &PNG::read_chunk_IHDR },
		{ IDAT, &PNG::read_chunk_IDAT },
		{ PLTE, &PNG::read_chunk_PLET },
		{ pHYs, &PNG::read_chunk_pHYs },
		{ iTXt, &PNG::read_chunk_null },
		{ tEXt, &PNG::read_chunk_null },
		{ IEND, &PNG::read_chunk_IEND }
	};

	const map<byte, void(PNG::*)()> color_type_parsers = {
		{ 0, &PNG::parse_color },
		{ 1, &PNG::parse_color },
		{ 2, &PNG::parse_color },
		{ 3, &PNG::parse_color },
		{ 4, &PNG::parse_color },
		{ 5, &PNG::parse_color },
		{ 6, &PNG::parse_color }
	};

	const map<byte, rgb_t(PNG::*)(pos_t)> rgb_filter_parsers = {
		{ 0, &PNG::filter_rgb_0 },
		{ 1, &PNG::filter_rgb_1 },
		{ 2, &PNG::filter_rgb_0 },
		{ 3, &PNG::filter_rgb_0 },
		{ 4, &PNG::filter_rgb_4 }
	};

	const map<size_t, rgba_t(PNG::*)(pos_t)> rgba_filter_parsers = {
		{ 0, &PNG::filter_rgba_0 },
		{ 1, &PNG::filter_rgba_1 },
		{ 2, &PNG::filter_rgba_2 },
		{ 3, &PNG::filter_rgba_3 },
		{ 4, &PNG::filter_rgba_4 }
	};

private:
	string m_output_path;

	ifstream m_img;
	byte_array m_buffer;			// buffer for reading

	// img attributes
	size_t m_size;					// size of file
	size_t m_width;					// width of image
	size_t m_height;				// height of image
	size_t m_bit_depth;				// bit depth
	size_t m_color_type;			// color type
	size_t m_compression_method;	// compression method
	size_t m_filter_method;			// filter method
	size_t m_interlace_method;		// interlace method
	size_t m_pixel_size;			// size of a pixel. == 3 for RGB, 4 for RGBA

	// reading params
	size_t m_pos;					// current position in file
	byte_array m_data;				// current read data
	size_t m_read_size;				// size of last read

	// chunk data
	size_t m_chunk_length;			// current chunk length
	byte_array m_chunk_type;		// current chunk type
		
	// IDAT data
	byte_array m_compressed_data;
	byte_array m_decompressed_data;
	color_array m_color_data;

	byte_array m_output;
};