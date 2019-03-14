#include <iostream>
#include <sstream>
#include <fstream>
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

  virtual clang::Expr* VisitUnaryOperator(clang::UnaryOperator* uop)
  {
    if (uop->getOpcode() == clang::UnaryOperatorKind::UO_Deref)
    {
      // check underlying declref
      auto* under_expr = uop->getSubExpr();
      if (clang::isa<clang::ImplicitCastExpr>(under_expr))
      {
        auto* deep_under_expr = clang::cast<clang::ImplicitCastExpr>(under_expr)->getSubExprAsWritten();
        if (clang::isa<clang::DeclRefExpr>(deep_under_expr))
        {
          auto* expr = clang::cast<clang::DeclRefExpr>(deep_under_expr);
          auto name = expr->getNameInfo().getName().getAsString();

          if (parameterNames_.find(name) != parameterNames_.end())
          {
            std::string newName = parameterNames_[name];
            std::cout << "\tRenaming: " << name << " -> " << newName << std::endl;

            rewriter_.ReplaceText(expr->getNameInfo().getSourceRange(), newName + ".read()");
          }
        }
      }
    }
    return uop;
  }

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
      std::size_t c = 0;
      std::vector<std::pair<std::uint64_t, std::size_t>> parms;
      for (clang::ParmVarDecl* param : func->parameters())
      {
        auto* type = param->getOriginalType().getTypePtr();
        assert(type->isArrayType());

        std::uint64_t arr_size = llvm::dyn_cast<clang::ConstantArrayType>(type->getAsArrayTypeUnsafe())->getSize().getLimitedValue();
        parms.emplace_back(arr_size, c++);
       // arr_size, param, param->getTypeSourceInfo()->getTypeLoc().getAs<clang::ConstantArrayTypeLoc>());
      }
      std::sort(parms.begin(), parms.end(), [](std::pair<std::uint64_t, std::size_t> a, std::pair<std::uint64_t, std::size_t> b)
                                            { return std::get<0>(a) < std::get<0>(b); });
      /*  Order should be:  ([7] appears only for globals)
       *   [1], [3], [2], [4], [5], [6], ([7])
       */
//      std::iter_swap(parms.begin() + 1, parms.begin() + 2);

      for (std::size_t i = 0; i < func->parameters().size(); ++i)
      {
        auto* old_par = func->parameters()[i];
        auto new_par = parms[i];
        std::size_t t = new_par.first;
        std::size_t j = new_par.second;
        
        std::string oldParName = old_par->getName().str();
        std::string newParName = (t == 1 ? "sub" :
                                 (t == 2 ? "query" :
                                 (t == 3 ? "subject_len" :
                                 (t == 4 ? "query_len" :
                                 (t == 5 ? "wbuff" : 
                                 (t == 6 ? "res" :
                                 (t == 7 ? "top_row" :  "__INVALID_NAME__")))))));
        std::cout << "Renaming: " << oldParName << " -> " << newParName << std::endl;

        // we need to match the oldParName in relation to i, not t
        parameterNames_[oldParName] = (j == 0 ? "sub" :
                                      (j == 1 ? "query" :
                                      (j == 2 ? "subject_len" :
                                      (j == 3 ? "query_len" :
                                      (j == 4 ? "wbuff" : 
                                      (j == 5 ? "res" :
                                      (j == 6 ? "top_row" :  "__INVALID_NAME__")))))));

        std::string oldType = old_par->getOriginalType().getAsString();
        std::string newType = (t == 1 ? "hls::stream<InputStreamType>&" :
                              (t == 2 ? "SequenceElem" : //[PE_COUNT]
                              (t == 3 || t == 4 ? "int" :
                              (t == 5 ? "hls::stream<OutputStreamType>&" :
                              (t == 6 ? "ScoreType*" :
                              (t == 7 ? "ScoreType" /*[PE_COUNT]*/ : "__INVALID_TYPE__"))))));
        std::cout << "Replacing: " << oldType << " -> " << newType << std::endl;

        std::string newDecl = newType + " " + newParName + (t != 2 && t != 7 ? "" : "[PE_COUNT]");
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

void add_include(std::string path);

class FunctionDeclFrontendAction : public clang::ASTFrontendAction
{
  std::string file;
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, clang::StringRef file) {
    this->file = file.str();
    return std::make_unique<FunctionDeclASTConsumer>(CI);
  }

  ~FunctionDeclFrontendAction()
  {
    add_include(file);
  }
};


using namespace clang::tooling;

int main(int argc, const char **argv)
{
	llvm::cl::OptionCategory MyToolCategory("anyseq_hls_postproc options");
	CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);

  std::stringstream ss;
  std::streambuf* old = std::cout.rdbuf(ss.rdbuf());

	ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
	Tool.run(newFrontendActionFactory<FunctionDeclFrontendAction>().get());

  std::cerr.rdbuf(old);
  ss.str();

  return 0;
}

void add_include(std::string path)
{
  std::stringstream ss;
  {
    std::ifstream inf(path);
    for(std::string str; std::getline(inf, str); ss << str << "\n") 
    {  }
  }
  {
    std::ofstream off(path);
    // adds the include "align.h"
    off << "#include \"align.h\"\n"; 
    for(std::string str; std::getline(ss, str); off << str << "\n")
    {  }
  }
}


