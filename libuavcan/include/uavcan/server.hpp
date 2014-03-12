/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#pragma once

#include <uavcan/internal/node/generic_publisher.hpp>
#include <uavcan/internal/node/generic_subscriber.hpp>

namespace uavcan
{

template <typename DataType_,
          typename Callback = void(*)(const ReceivedDataStructure<typename DataType_::Request>&, typename DataType_::Response&),
          unsigned int NumStaticReceivers = 2,
          unsigned int NumStaticBufs = 1>
class Server : public GenericSubscriber<DataType_, typename DataType_::Request, NumStaticReceivers, NumStaticBufs>
{
public:
    typedef DataType_ DataType;
    typedef typename DataType::Request RequestType;
    typedef typename DataType::Response ResponseType;

private:
    typedef GenericSubscriber<DataType, RequestType, NumStaticReceivers, NumStaticBufs> SubscriberType;
    typedef GenericPublisher<DataType, ResponseType> PublisherType;

    PublisherType publisher_;
    Callback callback_;
    uint32_t response_failure_count_;
    ResponseType response_;

    void handleReceivedDataStruct(ReceivedDataStructure<RequestType>& request)
    {
        if (try_implicit_cast<bool>(callback_, true))
        {
            response_ = ResponseType();  // The application needs newly initialized structure
            callback_(request, response_);
        }
        else
            handleFatalError("Invalid server callback");

        const int res = publisher_.publish(response_, TransferTypeServiceResponse, request.getSrcNodeID(),
            request.getTransferID());
        if (res <= 0)
        {
            UAVCAN_TRACE("Server", "Response publication failure: %i", res);
            response_failure_count_++;
        }
    }

public:
    Server(Scheduler& scheduler, IAllocator& allocator, IMarshalBufferProvider& buffer_provider)
    : SubscriberType(scheduler, allocator)
    , publisher_(scheduler, buffer_provider)
    , callback_()
    , response_failure_count_(0)
    {
        StaticAssert<DataTypeKind(DataType::DataTypeKind) == DataTypeKindService>::check();
    }

    int start(Callback callback)
    {
        stop();

        if (!try_implicit_cast<bool>(callback, true))
        {
            UAVCAN_TRACE("Server", "Invalid callback");
            return -1;
        }
        callback_ = callback;

        return SubscriberType::startAsServiceRequestListener();
    }

    using SubscriberType::stop;

    uint32_t getRequestFailureCount() const { return SubscriberType::getFailureCount(); }
    uint32_t getResponseFailureCount() const { return response_failure_count_; }
};

}