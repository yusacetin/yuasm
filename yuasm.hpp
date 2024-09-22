#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <memory>

class Yuasm {
    public:
        Yuasm(std::string first_fname);

    private:
        const int DEBUG_LEVEL = 1; // 0: nothing, 1: state completions, 2: full info

        enum State {
            SCAN_FIRST,
            SCAN_INSTR_OR_MACRO,
            SCAN_PARAMS,
            SCAN_PREPROC_DEF,
            SCAN_PREPROC_SUB,
            SCAN_INCLUDE_FPATH,
            SCAN_FUNC_HEAD,
            SCAN_PREPROC_VAL,
            WAIT_PAREN_CLOSE,
            COMMENT_SCAN_BEGIN,
            LINE_COMMENT,
            BLOCK_COMMENT,
            LINE_COMMENT_END,
            BLOCK_COMMENT_END
        };

        enum Input {
            UNKNOWN,
            AL,
            NUM,
            HASH,
            COMMA,
            DOT,
            COLON,
            LF,
            CR,
            SP,
            SC,
            SLASH,
            AST,
            INPUT_EOF,
            PAREN_OPEN,
            PAREN_CLOSE
        };

        int state = SCAN_FIRST;

        std::vector<char> buffer0; // for instructions and function names and macro names
        std::vector<char> buffer1; // for macro values and instruction parameters
        std::vector<std::string> params; // for instruction parameters

        std::stack<std::unique_ptr<std::ifstream>> files;
        std::map<std::string, std::string> macros;
        std::map<std::string, int> functions;
        int pc = 0; // program counter

        int state_before_block_comment; // TODO not properly implemented
        bool trailing_spaces_only = false;
        bool used_comma = false;

        int open_new_file(std::string fname);
        int mainloop();
        std::string print_state();
        bool is_valid_instr(std::string instr);
        int get_no_of_params_for_instr(std::string instr);

        static void expand_macro(std::vector<char>* buffer, std::map<std::string, std::string> macro_list);
        static const int get_category(char ch);
        static bool is_alphabetic(char ch);
        static bool is_numeric(char ch);
};