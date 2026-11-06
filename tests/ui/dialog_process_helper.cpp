#include <iostream>
#include <string>
int main(int argc, char** argv)
{
    std::string title, path;
    bool select = false, save = false, overwrite = false, filter = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg.starts_with("--title="))
            title = arg.substr(8);
        if (arg.starts_with("--filename="))
            path = arg.substr(11);
        if (arg.starts_with("--file-filter=Images | *.svg *.png"))
            filter = true;
        if (arg == "--file-selection")
            select = true;
        if (arg == "--save")
            save = true;
        if (arg == "--confirm-overwrite")
            overwrite = true;
    }
    if (!select)
        return 2;
    if (title == "cancel")
        return 1;
    if (title == "error")
        return 255;
    if (title == "invalid")
    {
        std::cout << "\xff\n";
        return 0;
    }
    if (title == "nul")
    {
        std::cout.write("/tmp/a\0b\n", 9);
        return 0;
    }
    if (title == "large")
    {
        std::cout << std::string(70000, 'x');
        return 0;
    }
    if (title == "save" && (!save || !overwrite || !filter))
        return 3;
    std::cout << (path.empty() ? "/tmp/vector gl \xCE\xA9.svg" : path) << '\n';
}
