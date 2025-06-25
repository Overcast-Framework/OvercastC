#include "ocpch.h"
#include "project_system.h"

using namespace std::literals;

std::string Overcast::ProjectSystem::Project::SerializeTOML()
{
    toml::table tbl;

    toml::table projData;
    projData.insert("ProjectName", ProjectName);
    projData.insert("ProjectVersion", ProjectVersion.to_string());
    projData.insert("LangVersion", ProjectVersion.to_string());

    toml::array depDirs;
    
    for (const auto& depDir : DependencyDirectories)
        depDirs.push_back(depDir);

    projData.insert("DependencyDirectories", depDirs);

    toml::table deps;
    for (const auto& dep : Dependencies)
    {
        deps.insert(dep.Name, dep.Version.to_string());
    }

    toml::table buildFlags;
    buildFlags.insert("no_std", no_std);
    buildFlags.insert("emit_llvm", emit_llvm);
    buildFlags.insert("no_autolink", skip_autolink);
    buildFlags.insert("OutputDirectory", outputFolder);

    tbl.insert("BuildInfo", buildFlags);
    tbl.insert("Dependencies", deps);
    tbl.insert("Project", projData);

    std::ostringstream outBuf;
    outBuf << tbl;

    return outBuf.str();
}

Overcast::ProjectSystem::Project Overcast::ProjectSystem::Project::LoadFromTOML(std::string toml)
{
    auto tbl = toml::parse(toml);
    
    Project p;
    p.ProjectName = tbl["Project"]["ProjectName"].value_or("<unknown>");
    p.ProjectVersion = Version::parse(tbl["Project"]["ProjectVersion"].value_or("1.0.0"));
    p.CompilerVersion = Version::parse(tbl["Project"]["LangVersion"].value_or("1.0.0"));
    
    auto depDirsArray = tbl["Project"]["DependencyDirectories"].as_array();
    for (const auto& dir : *depDirsArray)
    {
        p.DependencyDirectories.push_back(dir.as_string()->get());
    }

    for (const auto& i : *tbl["Dependencies"].as_table())
    {
        if (i.second.is_string())
            p.Dependencies.push_back({ std::string(i.first.str()), Version::parse(i.second.as_string()->get()) });
    }

    p.no_std = tbl["BuildInfo"]["no_std"].as_boolean();
    p.emit_llvm = tbl["BuildInfo"]["emit_llvm"].as_boolean();
    p.skip_autolink = tbl["BuildInfo"]["no_autolink"].as_boolean();
    p.outputFolder = tbl["BuildInfo"]["OutputDirectory"].as_string()->get();

    return p;
}

std::shared_ptr<Overcast::ProjectSystem::BuildResult> Overcast::ProjectSystem::BuildProcess::Build()
{
    try
    {
        std::ifstream inFile(this->buildFilePath, std::ios::binary);
        if (!inFile) {
            return std::make_shared<BuildResult>(BuildResult::BuildState::FAILURE, "Failed to open file " + this->buildFilePath);
        }

        std::string code(std::istreambuf_iterator<char>(inFile), {});

        auto tokens = LexAll(code);

        this->parser = Overcast::Parser::Parser(tokens);
        auto AST = this->parser.Parse();

        std::unordered_map<std::string, Overcast::Semantic::Binder::Symbol> symbols;

        for (const auto& stmt : AST)
        {
            if (auto funcDecl = dynamic_cast<FunctionDeclStatement*>(stmt.get()))
            {
                Overcast::Semantic::Binder::Symbol funcSymbol;
                funcSymbol.Name = funcDecl->FuncName;
                funcSymbol.Type = funcDecl->ReturnType.get();
                funcSymbol.ParamCount = funcDecl->Parameters.size();
                
                for (const auto& p : funcDecl->Parameters)
                {
                    funcSymbol.ParamTypeNames.push_back(p.ParameterType->to_string());
                    funcSymbol.ParamTypes.push_back(p.ParameterType.get());
                }

                funcSymbol.Kind = Overcast::Semantic::Binder::SymbolKind::Function;

                symbols[funcDecl->FuncName] = funcSymbol;
            }
            else if (auto structDecl = dynamic_cast<StructDeclStatement*>(stmt.get()))
            {
                Overcast::Semantic::Binder::Symbol strSymbol;
                strSymbol.Name = structDecl->StructName;
                strSymbol.Type = &IdentifierType{ structDecl->StructName };
                
                for (const auto& structMember : structDecl->Members)
                {
                    Overcast::Semantic::Binder::Symbol paramSymbol;
                    paramSymbol.Name = structMember.ParameterName;
                    paramSymbol.Type = structMember.ParameterType.get();
                    paramSymbol.Kind = Overcast::Semantic::Binder::SymbolKind::Variable;

                    strSymbol.StructSymbols.push_back(paramSymbol);
                }

                for (const auto& structFunction : structDecl->MemberFunctions)
                {
                    Overcast::Semantic::Binder::Symbol fSymbol;
                    fSymbol.Name = structFunction->FuncName;
                    fSymbol.Type = structFunction->ReturnType.get();
                    fSymbol.Kind = Overcast::Semantic::Binder::SymbolKind::Function;
                    fSymbol.IsStructMemberFunc = true;

                    for (const auto& p : structFunction->Parameters)
                    {
                        fSymbol.ParamTypeNames.push_back(p.ParameterType->to_string());
                    }

                    strSymbol.StructSymbols.push_back(fSymbol);
                }

                strSymbol.Kind = Overcast::Semantic::Binder::SymbolKind::Struct;
                symbols[strSymbol.Name] = strSymbol;
            }
        }
       /*this->binder.Run(AST);
        auto* module = this->codeGen.Generate(AST);

        if (EmitLLVM)
        {
            std::error_code EC;
            llvm::raw_fd_ostream llvmOut(this->buildFilePath+".ll", EC, llvm::sys::fs::OF_None);

            if (EC) {
                return { BuildResult::BuildState::FAILURE, "Failed to open file " + this->buildFilePath + ".ll" + " ("+EC.message()+")" };
            }
            module->print(llvmOut, nullptr);
        }

#ifdef _WIN32 // because windows prefers .obj and linux + others prefer .o lol
        auto objectLocation = "/obj/" + this->buildFilePath + ".obj";
#else
        auto objectLocation = "/obj/" + this->buildFilePath + ".o";
#endif

        this->codeGen.EmitToObjectFile(objectLocation, module);
        _isBuildComplete = true;
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "Building " << this->buildFilePath << "\n" << std::endl;
        }*/

        auto buildResult = std::make_shared<BuildResult>(BuildResult::BuildState::SUCCESS, this->buildFilePath + "> successfully compiled", "objectLocation", std::move(AST));
        buildResult->GlobalSymbols = symbols;

        return buildResult;
    }
    catch (Overcast::Parser::SyntaxError& error)
    {
        return std::make_shared<BuildResult>(BuildResult::BuildState::FAILURE, this->buildFilePath + "> " + error.what());
    }
    catch (std::runtime_error& error)
    {
        return std::make_shared<BuildResult>(BuildResult::BuildState::FAILURE, this->buildFilePath + "> " + error.what());
    }
    catch (...)
    {
        return std::make_shared<BuildResult>(BuildResult::BuildState::FAILURE, this->buildFilePath + ">" + " Something went wrong during the build process.");
    }
}

void Overcast::ProjectSystem::BuildSystem::AddBuildFile(const std::string& file, const std::vector<std::string>& deps)
{
    if (this->processes.find(file) != this->processes.end())
    {
        std::cerr << file + "> attempted to insert file for building twice" << std::endl;
    }

    this->processes.emplace(file, std::make_shared<BuildProcess>(file));
    this->dependencies.emplace(file, deps);
}

bool clangExists() {
    if (std::system("clang --version > nul 2>&1") == 0)
        return true;

#ifdef _WIN32
    std::string fallbackPath = "\"C:\\Program Files\\LLVM\\bin\\clang.exe\" --version > nul 2>&1";
#else
    std::string fallbackPath = "/usr/bin/clang --version > /dev/null 2>&1";
#endif

    return std::system(fallbackPath.c_str()) == 0;
}

Overcast::ProjectSystem::BuildResult Overcast::ProjectSystem::BuildSystem::RunBuild(std::string projectName, uint32_t numThreads)
{
    std::vector<std::shared_ptr<BuildProcess>> nonDeps;
    std::vector<std::shared_ptr<BuildProcess>> deps;

    for (const auto& buildProc : this->processes)
    {
        if (!dependencies.find(buildProc.first)->second.empty())
            deps.push_back(buildProc.second);
        else
            nonDeps.push_back(buildProc.second);
    }

    std::unordered_set<std::string> builtFiles;
    std::unordered_map<std::string, std::shared_future<std::shared_ptr<BuildResult>>> futures;

    ThreadPool threadPool(numThreads);

    for (auto& buildProc : nonDeps)
    {
        builtFiles.insert(buildProc->buildFilePath);
        futures[buildProc->buildFilePath] = threadPool.Submit([buildProc]() {
            return buildProc->Build();
            });
    }

    std::unordered_map<std::string, std::shared_future<std::shared_ptr<BuildResult>>> futuresMap;
    std::mutex futuresMutex;

    std::function<std::shared_future<std::shared_ptr<BuildResult>>(std::shared_ptr<BuildProcess>)> dependencyBuilder;

    dependencyBuilder = [&](std::shared_ptr<BuildProcess> proc) -> std::shared_future<std::shared_ptr<BuildResult>> {
        const auto& path = proc->buildFilePath;

        {
            std::lock_guard<std::mutex> lock(futuresMutex);
            auto it = futuresMap.find(path);
            if (it != futuresMap.end()) {
                return it->second;
            }
        }

        std::vector<std::shared_future<std::shared_ptr<BuildResult>>> depFutures;
        for (const auto& depPath : dependencies[path]) {
            depFutures.push_back(dependencyBuilder(processes[depPath]));
        }

        auto future = threadPool.Submit([proc, depFutures = std::move(depFutures)]() mutable -> std::shared_ptr<BuildResult>{
            for (auto& fut : depFutures) {
                auto res = fut.get();
                if (!res->IsSuccess()) {
                    return std::make_shared<BuildResult>(BuildResult::BuildState::FAILURE, "Dependency failed");
                }
            }

            return proc->Build();
            });

        {
            std::lock_guard<std::mutex> lock(futuresMutex);
            futuresMap[path] = future;
        }

        return future;
        };

    for (auto& buildProc : deps)
        dependencyBuilder(buildProc);

    threadPool.WaitAll();

    std::unordered_map<std::string, std::vector<std::unique_ptr<Statement>>> FileASTs;
    std::unordered_map<std::string, Overcast::Semantic::Binder::Symbol> GlobalSymbolTable;
    for (const auto& [path, future] : futures)
    {
        auto result = future.get();

        if (!result->IsSuccess())
            return { BuildResult::BuildState::FAILURE, result->GetErrors() };
        else
            std::cout << result->GetErrors() << std::endl; // not really an error, but

        FileASTs[path] = std::move(result->ASTresult);
        for (const auto& symbols : result->GlobalSymbols)
        {
            if(symbols.first != "main")
                GlobalSymbolTable[symbols.first] = symbols.second; // this shadows, but /w/
        }
    }

    std::filesystem::path cwd = std::filesystem::current_path();

    for (const auto& fileAST : FileASTs)
    {
        Overcast::Semantic::Binder::Binder binder(GlobalSymbolTable);
        Overcast::CodeGen::CGEngine codeGen(fileAST.first);

        binder.Run(fileAST.second);
        auto* module = codeGen.Generate(GlobalSymbolTable, fileAST.second);

        //module->print(llvm::errs(), nullptr);
        codeGen.EmitToObjectFile((cwd / "obj" / (std::filesystem::path(fileAST.first).filename().string() + ".obj")).string(), module);
        std::cout << fileAST.first << " -> " << (std::filesystem::path(fileAST.first).filename().string() + ".obj") << std::endl;
    }

#ifdef _WIN32
    std::string exeExt = ".exe";
#else
    std::string exeExt = "";
#endif

    std::string allFiles = "";

    for (std::filesystem::recursive_directory_iterator i(cwd/"obj", std::filesystem::directory_options::skip_permission_denied), end; i != end; ++i)
    {
        if (!std::filesystem::is_directory(i->path()))
        {
            if (i->path().extension() == ".obj")
                allFiles += i->path().string() + " ";
        }
    }

    if (clangExists())
    {
        std::string cmd = "clang " + allFiles + "-o bin/" + projectName + exeExt;
        if (std::system(cmd.c_str()) == EXIT_FAILURE)
        {
            std::cerr << "Failed to link project" << std::endl;
            return { BuildResult::BuildState::FAILURE, "failed to link project files" };
        }

        std::cout << projectName + " -> " + projectName + exeExt;
    }
    else
    {
        std::cout << "Clang was not detected or is not in PATH - this means your project could not be linked, and you must either install Clang and rerun the build process, or do it manually." << std::endl;
    }

    return { BuildResult::BuildState::SUCCESS };
}

Overcast::ProjectSystem::ThreadPool::ThreadPool(uint32_t numThreads)
{
    for (uint32_t i = 0; i < numThreads; ++i)
    {
        workers.emplace_back([this]() {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this]() { return stop || !tasks.empty(); });
                    if (stop && tasks.empty()) return;
                    task = std::move(tasks.front());
                    tasks.pop();
                    tasksInProgress++;
                }
                task();
                tasksInProgress--;
                condition.notify_all();
            }
        });
    }
}

Overcast::ProjectSystem::ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (auto& worker : workers)
        worker.join();
}

std::shared_future<std::shared_ptr<Overcast::ProjectSystem::BuildResult>> Overcast::ProjectSystem::ThreadPool::Submit(std::function<std::shared_ptr<BuildResult>()> task)
{
    auto pkgTask = std::make_shared<std::packaged_task<std::shared_ptr<BuildResult>()>>(task);
    std::future<std::shared_ptr<BuildResult>> future = pkgTask->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop)
            throw std::runtime_error("Submit on stopped ThreadPool");

        tasks.emplace([pkgTask]() {(*pkgTask)(); });
    }

    condition.notify_one();
    return future.share();
}

void Overcast::ProjectSystem::ThreadPool::WaitAll()
{
    std::unique_lock<std::mutex> lock(queueMutex);
    condition.wait(lock, [this]() { return tasks.empty() && tasksInProgress.load() == 0; });
}
