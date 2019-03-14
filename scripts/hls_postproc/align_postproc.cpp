#include <iostream>
#include <algorithm>
#include <map>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/AST/Expr.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

class FunctionDeclASTVisitor : public clang::RecursiveASTVisitor<FunctionDeclASTVisitor>
{
	clang::SourceManager& sourceManager_;
	clang::Rewriter& rewriter_;

  std::map<std::string, std::string> parameterNames_;

public:
	explicit FunctionDeclASTVisitor(clang::SourceManager& sm, clang::Rewriter& r)
		: sourceManager_(sm), rewriter_(r), parameterNames_()
	{ }

  virtual clang::Expr* VisitDeclRefExpr(clang::DeclRefExpr* expr)
  {
    auto name = expr->getNameInfo().getName().getAsString();

    if (parameterNames_.find(name) != parameterNames_.end())
    {
      std::string newName = parameterNames_[name];
      std::cout << "\tRenaming: " << name << " -> " << newName << std::endl;

      rewriter_.ReplaceText(expr->getNameInfo().getSourceRange(), newName);
    }
    return expr;
  }

	virtual bool VisitFunctionDecl(clang::FunctionDecl* func)
	{
		if (sourceManager_.isWrittenInMainFile(func->getSourceRange().getBegin()))
		{
      if (!func->doesThisDeclarationHaveABody())
      {
        std::cout << "Removing FuncDecl: " << func->getNameInfo().getName().getAsString() << std::endl;
        rewriter_.RemoveText(func->getSourceRange());
        return true;
      }
			std::string oldFuncName = func->getNameInfo().getName().getAsString();
			std::string newFuncName = "align";
			std::cout << "Renaming: " << oldFuncName << " -> " << newFuncName << std::endl;
			rewriter_.ReplaceText(func->getNameInfo().getSourceRange(), newFuncName);

      //sort params
      std::vector<std::tuple<std::uint64_t, clang::ParmVarDecl*, clang::ConstantArrayTypeLoc>> parms;
      for (clang::ParmVarDecl* param : func->parameters())
      {
        auto* type = param->getOriginalType().getTypePtr();
        assert(type->isArrayType());

        std::uint64_t arr_size = llvm::dyn_cast<clang::ConstantArrayType>(type->getAsArrayTypeUnsafe())->getSize().getLimitedValue();
        parms.emplace_back(arr_size, param, param->getTypeSourceInfo()->getTypeLoc().getAs<clang::ConstantArrayTypeLoc>());
      }
      std::sort(parms.begin(), parms.end(), [](std::tuple<std::uint64_t, clang::ParmVarDecl*, clang::ConstantArrayTypeLoc> a, std::tuple<std::uint64_t, clang::ParmVarDecl*, clang::ConstantArrayTypeLoc> b)
                                            { return std::get<0>(a) < std::get<0>(b); });
      /*  Order should be:  ([7] appears only for globals)
       *   [1], [3], [2], [4], [5], [6], ([7])
       */
      std::iter_swap(parms.begin() + 1, parms.begin() + 2);

      for (std::size_t i = 0; i < func->parameters().size(); ++i)
      {
        auto* old_par = func->parameters()[i];
        auto new_par = parms[i];
        
        std::string oldParName = old_par->getName().str();
        std::string newParName = (i == 0 ? "sub" :
                                 (i == 1 ? "query" :
                                 (i == 2 ? "subject_len" :
                                 (i == 3 ? "query_len" :
                                 (i == 4 ? "wbuff" : 
                                 (i == 5 ? "res" :
                                 (i == 6 ? "top_row" :  "__INVALID_NAME__")))))));
        std::cout << "Renaming: " << oldParName << " -> " << newParName << std::endl;
        parameterNames_[oldParName] = newParName;

        std::string oldType = old_par->getOriginalType().getAsString();
        std::string newType = (i == 0 ? "hls::stream<InputStreamType>&" :
                              (i == 1 ? "SequenceElem" : //[PE_COUNT]
                              (i == 2 || i == 3 ? "int" :
                              (i == 4 ? "hls::stream<OutputStreamType>&" :
                              (i == 5 ? "ScoreType" :
                              (i == 6 ? "ScoreType" /*[PE_COUNT]*/ : "__INVALID_TYPE__"))))));
        std::cout << "Replacing: " << oldType << " -> " << newType << std::endl;

        std::string newDecl = newType + " " + newParName + (i != 1 && i != 6 ? "" : "[PE_COUNT]");
//        rewriter_.ReplaceText(std::get<1>(new_par)->getSourceRange(), newDecl);
        rewriter_.ReplaceText(old_par->getSourceRange(), newDecl);
      }
		}
		return true;
	}
};

class FunctionDeclASTConsumer : public clang::ASTConsumer
{
	clang::Rewriter rewriter_;
	FunctionDeclASTVisitor visitor_; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit FunctionDeclASTConsumer(clang::CompilerInstance& ci)
		: visitor_(ci.getSourceManager(), rewriter_)
	{ }
 
	virtual void HandleTranslationUnit(clang::ASTContext& astContext)
	{
		rewriter_.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
		visitor_.TraverseDecl(astContext.getTranslationUnitDecl());
		rewriter_.overwriteChangedFiles();
	}
};

class FunctionDeclFrontendAction : public clang::ASTFrontendAction
{
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, clang::StringRef file) {
    return std::make_unique<FunctionDeclASTConsumer>(CI);
  }
};


using namespace clang::tooling;

int main(int argc, const char **argv)
{
	llvm::cl::OptionCategory MyToolCategory("anyseq_hls_postproc options");
	CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);

	ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
	Tool.run(newFrontendActionFactory<FunctionDeclFrontendAction>().get());

    return 0;
}

