/**
 * @file HsException.cpp
 * @brief HsException实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsException.h>
#include <HsCommData/HsSystem.h>
#include <HsCommData/HsTuple.h>

namespace HsCommData {

namespace {

// 按错误码取主说明
const char* messageOf(HsError code)
{
    switch (code) {
    case HS_OK:
        return "Completed successfully";
    case HS_ERR_INDEX:
        return "Index out of range";
    case HS_ERR_TYPE:
        return "Type mismatch";
    case HS_ERR_INT_RANGE:
        return "Integer out of 32-bit range";
    case HS_ERR_NULL:
        return "Output pointer is null";
    case HS_ERR_PARAM:
        return "Tool parameter is missing";
    case HS_ERR_ACTION:
        return "Action is not supported";
    case HS_ERR_NO_ALG:
        return "Algorithm is not registered";
    case HS_ERR_OPERATOR:
        return "Operator failed";
    case HS_ERR_IMAGE_ROW:
        return "Image row out of range";
    case HS_ERR_IMAGE_EMPTY:
        return "No image or invalid size";
    case HS_ERR_FILE_OPEN:
        return "Cannot open file";
    case HS_ERR_FILE_IO:
        return "File read or write failed";
    case HS_ERR_FILE_FORMAT:
        return "File format or version mismatch";
    case HS_ERR_TMPL_NO_TEACH:
        return "No teach region";
    case HS_ERR_TMPL_NO_MODEL:
        return "No model";
    case HS_ERR_TMPL_NO_MATCH:
        return "No match";
    case HS_ERR_TMPL_OPERATOR:
        return "Locate operator failed";
    case HS_ERR_TMPL_NO_DIR:
        return "No tool data directory";
    case HS_ERR_TMPL_METHOD:
        return "Unknown template method";
    case HS_ERR_BLOB_NO_REGION:
        return "No connected region";
    case HS_ERR_BLOB_OPERATOR:
        return "Blob operator failed";
    case HS_ERR_LINE_NONE:
        return "No line found";
    case HS_ERR_LINE_OPERATOR:
        return "Find line operator failed";
    case HS_ERR_STITCH_FEW:
        return "Need at least two images";
    case HS_ERR_STITCH_OPERATOR:
        return "Stitch operator failed";
    case HS_ERR_QR_NONE:
        return "No QR code";
    case HS_ERR_QR_OPERATOR:
        return "QR operator failed";
    case HS_ERR_BAR_NONE:
        return "No barcode";
    case HS_ERR_BAR_OPERATOR:
        return "Barcode operator failed";
    case HS_ERR_ACQUIRE_NO_IMAGE:
        return "No acquired image";
    case HS_ERR_ACQUIRE_CONVERT:
        return "HsImage to HObject failed";
    default:
        return "Unknown error";
    }
}

// 空指针当空串
std::string takeText(const char* text)
{
    if (!text) {
        return std::string();
    }
    return std::string(text);
}

}

struct HsException::Data {
    std::string proc; ///< 算子名。没有则为空串
    std::string message; ///< 主说明
    std::string extendedMessage; ///< 附加说明。没有则为空串
    std::string whatText; ///< what() 用的整句说明
    HsError code; ///< 错误码
    HsError extendedCode; ///< 附加错误码。HS_OK 表示没有
    HsTuple userData; ///< 用户元组。没有附加数据时为空

    Data();
    /** @brief 按算子名、主说明和附加说明重拼 what */
    void rebuild();
};

// 错误码和附加码默认为成功
HsException::Data::Data()
    : code(HS_OK)
    , extendedCode(HS_OK)
{
}

// 有算子名放前面，有附加说明接在后面
void HsException::Data::rebuild()
{
    whatText.clear();
    if (!proc.empty()) {
        whatText += proc;
        whatText += ": ";
    }
    whatText += message;
    if (!extendedMessage.empty()) {
        whatText += " ";
        whatText += extendedMessage;
    }
}

// 填算子名、错误码和主说明，并重拼 what
void HsException::init(const char* procName, HsError errorCode, const char* message)
{
    m_data->proc = takeText(procName);
    m_data->code = errorCode;
    if (message && message[0] != '\0') {
        m_data->message = message;
    } else {
        m_data->message = messageOf(errorCode);
    }
    m_data->extendedCode = HS_OK;
    m_data->extendedMessage.clear();
    m_data->rebuild();
}

// 按错误码构造
HsException::HsException(HsError errorCode)
    : std::runtime_error("")
    , m_data(new Data())
{
    init(0, errorCode, 0);
}

// 带算子名和错误码
HsException::HsException(const char* procName, HsError errorCode)
    : std::runtime_error("")
    , m_data(new Data())
{
    init(procName, errorCode, 0);
}

// 带算子名和自定义说明
HsException::HsException(const char* procName, const char* message, HsError errorCode)
    : std::runtime_error("")
    , m_data(new Data())
{
    init(procName, errorCode, message);
}

// 深拷说明和用户元组
HsException::HsException(const HsException& other)
    : std::runtime_error(other)
    , m_data(new Data(*other.m_data))
{
}

// 深拷说明和用户元组
HsException& HsException::operator=(const HsException& other)
{
    if (this != &other) {
        std::runtime_error::operator=(other);
        Data* copied = new Data(*other.m_data);
        if (m_data) {
            delete m_data;
            m_data = nullptr;
        }
        m_data = copied;
    }
    return *this;
}

// 跨 DLL 边界释放内部数据
HsException::~HsException()
{
    if (m_data) {
        delete m_data;
        m_data = nullptr;
    }
}

// 按实际类型再抛出
void HsException::throwInstance() const
{
    throw *this;
}

// 按系统参数决定抛出、回调或继续
void raiseException(const HsException& exception)
{
    const HsSystem::ExceptionMode mode = HsSystem::instance().exceptionMode;
    if (mode == HsSystem::ExModeIgnore) {
        return;
    }
    if (mode == HsSystem::ExModeCallback) {
        const HsSystem::ExceptionCallback callback = HsSystem::instance().exceptionCallback;
        if (callback) {
            callback(exception);
            return;
        }
    }
    exception.throwInstance();
}

// 抛出只带错误码的异常
void HsException::Throw(HsError errorCode)
{
    raiseException(HsException(errorCode));
}

// 抛出带算子名和错误码的异常
void HsException::Throw(const char* procName, HsError errorCode)
{
    raiseException(HsException(procName, errorCode));
}

// 抛出带算子名和自定义说明的异常
void HsException::Throw(const char* procName, const char* message, HsError errorCode)
{
    raiseException(HsException(procName, message, errorCode));
}

// 算子或函数名
const char* HsException::procName() const
{
    return m_data->proc.c_str();
}

// 主说明
const char* HsException::errorMessage() const
{
    return m_data->message.c_str();
}

// 错误码
HsError HsException::code() const
{
    return m_data->code;
}

// 附加错误码
HsError HsException::extendedCode() const
{
    return m_data->extendedCode;
}

// 附加说明
const char* HsException::extendedMessage() const
{
    return m_data->extendedMessage.c_str();
}

// 用户元组
const HsTuple& HsException::userData() const
{
    return m_data->userData;
}

// 补上附加码和附加说明
void HsException::setExtended(HsError extendedCode, const char* extendedMessage)
{
    m_data->extendedCode = extendedCode;
    m_data->extendedMessage = takeText(extendedMessage);
    m_data->rebuild();
}

// 换上用户元组
void HsException::setUserData(const HsTuple& data)
{
    m_data->userData = data;
}

// 整句说明
const char* HsException::what() const noexcept
{
    return m_data->whatText.c_str();
}

// 类型不符，说明为元组访问失败
HsTupleAccessException::HsTupleAccessException()
    : HsException(static_cast<const char*>(nullptr), "Tuple access failed", HS_ERR_TYPE)
{
}

// 自定义说明，错误码为类型不符
HsTupleAccessException::HsTupleAccessException(const char* message)
    : HsException(static_cast<const char*>(nullptr), message, HS_ERR_TYPE)
{
}

// 按错误码构造
HsTupleAccessException::HsTupleAccessException(HsError errorCode)
    : HsException(errorCode)
{
}

// 自定义说明和错误码
HsTupleAccessException::HsTupleAccessException(const char* message, HsError errorCode)
    : HsException(static_cast<const char*>(nullptr), message, errorCode)
{
}

// 跨 DLL 边界释放内部数据
HsTupleAccessException::~HsTupleAccessException()
{
}

// 按本类型再抛出
void HsTupleAccessException::throwInstance() const
{
    throw *this;
}

// 抛出默认的元组访问异常
void HsTupleAccessException::Throw()
{
    raiseException(HsTupleAccessException());
}

// 抛出带说明的元组访问异常
void HsTupleAccessException::Throw(const char* message)
{
    raiseException(HsTupleAccessException(message));
}

// 抛出带错误码的元组访问异常
void HsTupleAccessException::Throw(HsError errorCode)
{
    raiseException(HsTupleAccessException(errorCode));
}

}
