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
        const int DEBUG_LEVEL = 0; // 0: instr info, 1: state completions, 2: full info

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
            PAREN_CLOSE,
            DASH
        };

        int state = SCAN_FIRST;

        std::vector<char> buffer0; // for instructions and function names and macro names
        std::vector<char> buffer1; // for macro values and instruction parameters
        std::vector<std::string> params; // for instruction parameters
        std::vector<unsigned int> instructions;
        std::vector<char> line_buffer; // used in error messages
        std::stack<int> line_counters; // used in error messages (a counter per file)
        std::stack<std::string> fnames; // used in error messages

        std::string ofname;
        std::stack<std::unique_ptr<std::ifstream>> files;
        std::map<std::string, std::string> macros;
        std::map<std::string, int> functions; // should be called sections really
        std::multimap<std::string, int> callers; // caller positions
        int pc = 0; // program counter

        int state_before_block_comment; // TODO not properly implemented
        bool trailing_spaces_only = false;
        bool used_comma = false;
        bool used_dash = false; // to detect negative numbers

        int open_new_file(std::string fname);
        int mainloop();
        std::string print_state();
        int eval_instr(std::string instr, std::vector<std::string> params);
        int get_function_index(std::string func);
        int write_object();
        int link_object();
        void print_line_to_std_err();

        static void expand_macro(std::vector<char>* buffer, std::map<std::string, std::string> macro_list);
        static const int get_category(char ch);
        static bool is_alphabetic(char ch);
        static bool is_numeric(char ch);
        static int get_no_of_params_for_instr(std::string instr); // returns -1 if instruction is invalid
        static unsigned int param_to_int(std::string param);
        static bool is_hex_digit(char c);
        static unsigned int get_hex_value(char c);
        static std::string get_instr_as_hex(unsigned int instr_int);
        static unsigned int twos_complement(unsigned int val);
        static std::string generate_ofname(std::string fpath);
};
