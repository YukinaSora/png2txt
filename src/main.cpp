import Module;
import std;

int main(int argc, char** argv, char** env)
{
	if (argc < 3) {
		std::cout << "Usage: png2txt <input file> <output path>" << std::endl;
		return 1;
	}
	PNG png{ argv[1], argv[2]};
	png.convert();

	return 0;
}
