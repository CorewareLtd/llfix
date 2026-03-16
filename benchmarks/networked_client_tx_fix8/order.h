#ifndef _ORDER_H_
#define _ORDER_H_

#include <cstdint>
#include <string>
#include <sstream>

enum class OrderType
{
    NONE,
    LIMIT,
    MARKET,             // IMPLICITY "IMMEDIATE OR CANCEL" TIME IN FORCE
    STOP_LOSS,
    PEGGED,
    ICEBERG,
    DARK,
    AUCTION_OPEN,
    AUCTION_MIDDAY,
    AUCTION_CLOSE,
    CONDITIONAL,
    QUOTE,
    RFQ,
};

static std::string convert_order_type_to_string(const OrderType type)
{
    switch (type)
    {
        case OrderType::NONE:
            return "NONE";
        case OrderType::LIMIT:
            return "LIMIT";
        case OrderType::MARKET:
            return "MARKET";
        case OrderType::STOP_LOSS:
            return "STOP LOSS";
        case OrderType::PEGGED:
            return "PEGGED";
        case OrderType::ICEBERG:
            return "ICEBERG";
        case OrderType::DARK:
            return "DARK";
        case OrderType::AUCTION_OPEN:
            return "AUCTION OPEN";
        case OrderType::AUCTION_MIDDAY:
            return "AUCTION MIDDAY";
        case OrderType::AUCTION_CLOSE:
            return "AUCTION CLOSE";
        case OrderType::CONDITIONAL:
            return "CONDITIONAL";
        case OrderType::QUOTE:
            return "QUOTE";
        case OrderType::RFQ:
            return "RFQ";
        default:
            return "UNKNOWN";
    }
}

enum class TimeInForce
{
    NONE,
    GFD,    // GOOD FOR DAY
    GTC,    // GOOD TILL CANCEL
    OPG,    // AT THE OPEN AUCTION
    FOK,    // FILL OR KILL
    IOC,    // IMMEDIATE OR CANCEL , THE DIFFERENCE FROM FOK IS THAT YOU MAY GET PARTIAL FILL
    GTX,    // GOOD TILL CROSSING
    GTD,    // GOOD TILL DATE
    CLS,    // AT THE CLOSE AUCTION
};

static std::string convert_tif_to_string(const TimeInForce tif)
{
    switch (tif)
    {
            case TimeInForce::NONE:
                return "NONE";
            case TimeInForce::GFD:
                return "GOOD FOR DAY";
            case TimeInForce::GTC:
                return "GOOD TILL CANCEL";
            case TimeInForce::OPG:
                return "AT THE OPEN AUCTION";
            case TimeInForce::FOK:
                return "FILL OR KILL";
            case TimeInForce::IOC:
                return "IMMEDIATE OR CANCEL";
            case TimeInForce::GTX:
                return "GOOD TILL CROSSING";
            case TimeInForce::GTD:
                return "GOOD TILL DATE";
            case TimeInForce::CLS:
                return "AT THE CLOSE AUCTION";
            default:
                return "UNKNOWN";
    }
}

enum class OrderState
{
    NONE,
    PENDING_NEW_ORDER,
    PENDING_REPLACE_ORDER,
    PENDING_CANCEL_ORDER,
    NEW_ORDER,
    REPLACED,
    CANCELLED,
    PARTIALLY_FILLED,
    FILLED,
    REJECTED_BY_VENUE,
    CANCELLED_BY_VENUE,     // Unsolicited cancellations
    DOD,                    // Done for day
    INVALID,                // Internally used. Ex: orig orders which are replaced will have state 'invalid; and state of the new order will be 'replaced'
};

static std::string convert_order_state_to_string(const OrderState state)
{
    std::string ret;

    if (state == OrderState::NONE)
    {
        ret = "NONE";
    }
    else if (state == OrderState::PENDING_NEW_ORDER)
    {
        ret = "PENDING_NEW_ORDER";
    }
    else if (state == OrderState::PENDING_REPLACE_ORDER)
    {
        ret = "PENDING_REPLACE_ORDER";
    }
    else if (state == OrderState::PENDING_CANCEL_ORDER)
    {
        ret = "PENDING_CANCEL_ORDER";
    }
    else if (state == OrderState::NEW_ORDER)
    {
        ret = "NEW_ORDER";
    }
    else if (state == OrderState::REPLACED)
    {
        ret = "REPLACED";
    }
    else if (state == OrderState::CANCELLED)
    {
        ret = "CANCELLED";
    }
    else if (state == OrderState::PARTIALLY_FILLED)
    {
        ret = "PARTIALLY_FILLED";
    }
    else if (state == OrderState::FILLED)
    {
        ret = "FILLED";
    }
    else if (state == OrderState::REJECTED_BY_VENUE)
    {
        ret = "REJECTED_BY_VENUE";
    }
    else if (state == OrderState::CANCELLED_BY_VENUE)
    {
        ret = "CANCELLED_BY_VENUE";
    }
    else if (state == OrderState::DOD)
    {
        ret = "DOD";
    }
    else if (state == OrderState::INVALID)
    {
        ret = "INVALID";
    }

    return ret;
}

enum class OrderSide
{
    NONE,
    BUY,
    SELL,
    SHORT_SELL
};

static std::string convert_order_side_to_string(const OrderSide side)
{
    std::string ret;

    if (side == OrderSide::NONE)
    {
        ret = "NONE";
    }
    else if (side == OrderSide::BUY)
    {
        ret = "BUY";
    }
    else if (side == OrderSide::SELL)
    {
        ret = "SELL";
    }
    else if (side == OrderSide::SHORT_SELL)
    {
        ret = "SHORT_SELL";
    }

    return ret;
}

class Order
{
    public:
        Order() = default;
        ~Order() = default;

        void set_type(OrderType type)
        {
            m_type = type;
        }

        OrderType get_type() const { return m_type; }

        void set_numeric_order_id(uint32_t numeric_order_id)
        {
            m_numeric_order_id = numeric_order_id;
        }

        uint32_t get_numeric_order_id()
        {
            return m_numeric_order_id;
        }

        void set_side(OrderSide side)
        {
            m_side = side;
        }

        OrderSide get_side() const
        {
            return m_side;
        }

        void set_symbol(const std::string& symbol)
        {
            m_symbol = symbol;
        }

        const std::string& get_symbol() const
        {
            return m_symbol;
        }

        void set_state(OrderState state)
        {
            m_state = state;
        }

        OrderState get_state() const
        {
            return m_state;
        }

        void set_tif(TimeInForce tif)
        {
            m_tif = tif;
        }

        TimeInForce get_tif() const
        {
            return m_tif;
        }

        void set_price(uint64_t price)
        {
            m_price = price;
        }

        uint64_t get_price()
        {
            return m_price;
        }

        void set_remaining_qty(uint32_t qty)
        {
            m_remaining_quantity = qty;
        }

        uint32_t get_remaining_qty() const
        {
            return m_remaining_quantity;
        }

        void set_filled_qty(uint32_t qty)
        {
            m_filled_quantity = qty;
        }

        uint32_t get_filled_qty() const
        {
            return m_filled_quantity;
        }

        void set_cancelled_qty(uint32_t qty)
        {
            m_cancelled_quantity = qty;
        }

        uint32_t get_cancelled_qty() const
        {
            return m_cancelled_quantity;
        }

        // Full cancellation
        void process_cancellation()
        {
            m_cancelled_quantity = m_remaining_quantity;
            m_remaining_quantity = 0;
            set_state(OrderState::CANCELLED);
        }

        // May be partial or full cancellation
        void process_quantity_cancellation(uint32_t cancelled_quantity)
        {
            m_cancelled_quantity += cancelled_quantity;
            m_remaining_quantity -= cancelled_quantity;
            update_order_state_based_on_quantities();
        }

        void process_execution(uint32_t executed_quantity, uint64_t execution_price)
        {
            (void)(execution_price);
            ///////////////////////////////////////////////////////////////////////////
            // Process quantity
            m_filled_quantity += executed_quantity;
            m_remaining_quantity -= executed_quantity;

            ///////////////////////////////////////////////////////////////////////////
            // Process state
            update_order_state_based_on_quantities();
        }

        void update_order_state_based_on_quantities()
        {
            if (m_remaining_quantity == 0)
            {
                if(m_filled_quantity>0)
                {
                    set_state(OrderState::FILLED);
                }
                else if (m_cancelled_quantity>0)
                {
                    set_state(OrderState::CANCELLED);
                }
            }
            else
            {
                if(m_filled_quantity>0)
                {
                    set_state(OrderState::PARTIALLY_FILLED);
                }
                else
                {
                    set_state(OrderState::NEW_ORDER);
                }
            }
        }

        std::string get_display_text()
        {
            std::stringstream stream;

            stream << "Numeric order id : " << m_numeric_order_id << "\n";

            stream << "State : " << convert_order_state_to_string(m_state) << "\n";
            stream << "Type : " << convert_order_type_to_string(m_type) << "\n";
            stream << "Side : " << convert_order_side_to_string(m_side) << "\n";
            stream << "Time in force : " << convert_tif_to_string(m_tif) << "\n";
            stream << "Price :" << std::to_string(m_price) << "\n";
            stream << "Symbol : " << m_symbol << "\n";
            stream << "Remaining qty : " << m_remaining_quantity << "\n";
            stream << "Filled qty : " << m_filled_quantity << "\n";
            stream << "Cxled qty : " << m_cancelled_quantity << "\n";

            return stream.str();
        }

    private:
        uint32_t m_numeric_order_id = 0;
        OrderType m_type = OrderType::NONE;
        OrderState m_state = OrderState::NONE;
        OrderSide m_side = OrderSide::NONE;
        TimeInForce m_tif = TimeInForce::NONE;
        std::string m_symbol;
        uint32_t m_remaining_quantity = 0;
        uint32_t m_filled_quantity = 0;
        uint32_t m_cancelled_quantity = 0;
        uint64_t m_price;
};

#endif