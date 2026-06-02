#include "includes/Errors.hpp"

int     main( void )
{
    try {

        throw (Error("test"));
    }
    catch (std::exception &e)
    {
        std::cout << "Error : " << e.what() << std::endl;
    }
    return 0;
}