#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <string>
#include <exception>

class Exception : public std::exception
{
    public:
        Exception(const std::string& msg):
            m_msg(msg)
        {}

        virtual ~Exception() noexcept;

        virtual const char* what() const noexcept
        {
           return m_msg.c_str();
        }

    protected:
        std::string m_msg;
};

class AudioException : public Exception
{
    public:
        AudioException(const std::string& msg):
            Exception(msg)
        {}
};

class AtspiException : public Exception
{
    public:
        AtspiException(const std::string& msg):
            Exception(msg)
        {}
};

class KeyDefsException : public Exception
{
    public:
        KeyDefsException(const std::string& msg):
            Exception(msg)
        {}
};

class LayoutException : public Exception
{
    public:
        LayoutException(const std::string& msg):
            Exception(msg)
        {}
};

class ProcessException : public Exception
{
   public:
       ProcessException(const std::string& msg):
           Exception(msg)
       {}
};

class SchemaException : public Exception
{
   public:
       SchemaException(const std::string& msg):
           Exception(msg)
       {}
};

class SVGException : public Exception
{
    public:
        SVGException(const std::string& msg):
            Exception(msg)
        {}
};

class UDevException : public Exception
{
    public:
        UDevException(const std::string& msg):
            Exception(msg)
        {}
};

class ValueException : public Exception
{
    public:
        ValueException(const std::string& msg):
            Exception(msg)
        {}
};

class VirtKeyException : public Exception
{
    public:
        VirtKeyException(const std::string& msg):
            Exception(msg)
        {}
};

#endif // EXCEPTION_H
