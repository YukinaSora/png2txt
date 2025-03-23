import Module;
import std;

struct ProgramOptions 
{
    std::string input_file;
    std::string output_path = "./";
    bool enable_txt = false;
    bool enable_html = false;
    bool enable_canvas = false;
};

void print_usage()
{
    std::cout << "Usage: " << " -f <file> [options]\n"
        << "Options:\n"
        << "  -h            Show this usage\n"
        << "    --help\n"
        << "  -f <file>     Specify input file\n"
        << "    --filename\n"
        << "  -o <path>     Set output directory\n"
        << "    --output_path\n"
        << "  --txt         Enable TXT output\n"
        << "  --html        Enable HTML output\n"
        << "  --canvas      Enable Canvas output\n";
}

ProgramOptions parse_arguments(int argc, char* argv[]) 
{
    constexpr int EXIT_SUCCESS = 0;
    constexpr int EXIT_FAILURE = 1;

    ProgramOptions options;
    std::vector<std::string> args(argv + 1, argv + argc);  // 跳过程序名

    if (argc == 1 || args[0] == "-h" || args[0] == "--help") {
        print_usage();
        exit(EXIT_SUCCESS);
    }

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-f" || args[i] == "--filename") {
            if (i + 1 < args.size()) {
                options.input_file = args[++i];  // 获取下一个参数作为文件名
            }
            else {
                std::cerr << "Error: -f requires a filename argument" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (args[i] == "-o" || args[i] == "--output_path") {
            if (i + 1 < args.size()) {
                options.output_path = args[++i];  // 获取输出路径
            }
            else {
                std::cerr << "Error: -o requires an output path" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (args[i] == "--txt") {
            options.enable_txt = true;
        }
        else if (args[i] == "--html") {
            options.enable_html = true;
        }
        else if (args[i] == "--canvas") {
            options.enable_canvas = true;
        }
        else {
            std::cerr << "Unknown option: " << args[i] << std::endl;
            print_usage();
            exit(EXIT_FAILURE);
        }
    }

    // 校验必须参数
    if (options.input_file.empty()) {
        std::cerr << "Error: Input file (-f) is required" << std::endl;
        print_usage();
        exit(EXIT_FAILURE);
    }

    return options;
}

int main(int argc, char** args, char** env)
{
    PNG png{ };

    auto arguments = parse_arguments(argc, args);
    png.open(arguments.input_file);
    png.save_to(arguments.output_path);
    png.enable_output_txt(arguments.enable_txt);
    png.enable_output_html_ascii(arguments.enable_html);
    png.enable_output_html_canvas(arguments.enable_canvas);

	png.convert();

	return 0;
}
