#include <iostream>
#include <fstream>
#include <filesystem>
#include "OCLReader.h"

void PrintHelp()
{
	std::cout << "Usage: overclad -i lexer_spec.ocl -o <output_dir?>" << std::endl;
	std::cout << "If -o is not specified it creates the lexer.h and lexer.cc files in the current working directory." << std::endl;
}

bool FileExists(const std::string& filename)
{
	return std::filesystem::exists(filename);
}

int main(int argc, char** argv)
{
	if (argc > 1)
	{
		std::string lexerSpecFile = "";
		std::string outputDir = "";

		for (int i = 1; i < argc; i++)
		{
			std::string option = argv[i];
			if (option == "-i")
			{
				if (i + 1 < argc)
				{
					lexerSpecFile = argv[i + 1];

					if (!FileExists(lexerSpecFile))
					{
						std::cout << "Lexer specification file \"" << lexerSpecFile << "\" is missing or invalid." << std::endl;
						return 1;
					}

					i++;
				}
				else
				{
					std::cout << "Lexer specification file not specified for switch -i." << std::endl;
					PrintHelp();
					return 1;
				}
			}
			else if (option == "-o")
			{
				if (i + 1 < argc)
				{
					outputDir = argv[i + 1];
					i++;
				}
				else
				{
					std::cout << "Output directory not specified for switch -o." << std::endl;
					PrintHelp();
					return 1;
				}
			}
			else
			{
				std::cout << "Unknown switch " << option << std::endl;
				PrintHelp();
				return 1;
			}
		}

		if (!lexerSpecFile.empty()) // perfect
		{
			if (outputDir.empty())
			{
				std::cout << "[LOG]: Output directory was not specified, working in " << std::filesystem::current_path() << std::endl;
				outputDir = std::filesystem::current_path().generic_string();
			}

			Overclad::OCLAnalysis::OCLReader oclReader;
			oclReader.GenerateCXXCode(lexerSpecFile);

			if (!std::filesystem::exists(outputDir))
				std::filesystem::create_directory(outputDir);

			std::ofstream headerOut(outputDir + '/' + "lexer.h");
			headerOut << oclReader.m_HGenFeed.str();
			headerOut.close();

			std::ofstream ccOut(outputDir + '/' + "lexer.cc");
			ccOut << oclReader.m_GenFeed.str();
			ccOut.close();

			std::cout << "Lexer creation finished." << std::endl;
		}
		else
		{
			std::cout << "IDP: Lexer specification file path was empty." << std::endl; // not sure how this would happen, but edgecases.
		}
	}
	else
	{
		PrintHelp();
	}

	return 0;
}