#include <exception>
#include <iostream>


class Error : public std::exception
{
    private:
        std::string     _msg;
    public:
        Error(const std::string &msg) throw() : _msg(msg){};
        virtual ~Error() throw() {};
        virtual const char *what() const throw() { return _msg.c_str(); }
};