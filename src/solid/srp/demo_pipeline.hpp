#pragma once
// =============================================================================
//  demo_pipeline.hpp — SRP em um pipeline de processamento de dados
// =============================================================================
//
//  Cenário realista: processar um CSV de pedidos de e-commerce.
//  Etapas do pipeline:
//    1. Leitura / parse do CSV          → CsvParser
//    2. Validação das regras de negócio → OrderValidator
//    3. Cálculo de totais e impostos    → PricingEngine
//    4. Geração de nota fiscal          → InvoiceBuilder
//    5. Envio de notificação            → NotificationService
//
//  Cada etapa é uma classe com exatamente uma responsabilidade.
//  O pipeline (OrderProcessor) apenas orquestra — não implementa nada.
//
//  Benefício imediato: podemos testar cada etapa isoladamente e trocar
//  qualquer implementação sem alterar as demais.
//
// =============================================================================

#include <fmt/core.h>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <optional>
#include <numeric>

namespace demo_pipeline {

// ─────────────────────────────────────────────────────────────────────────────
//  Modelos de domínio (PODs — sem comportamento, só dados)
// ─────────────────────────────────────────────────────────────────────────────
struct OrderItem {
    std::string sku;
    int         quantity;
    double      unit_price;
};

struct Order {
    std::string             id;
    std::string             customer_email;
    std::vector<OrderItem>  items;
};

struct Invoice {
    std::string order_id;
    double      subtotal;
    double      tax;
    double      total;
    std::string customer_email;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Etapa 1: CsvParser
//  Razão única para mudar: mudança no formato do arquivo de entrada.
// ─────────────────────────────────────────────────────────────────────────────
class CsvParser {
public:
    // CSV simulado:
    // order_id, customer_email, sku, quantity, unit_price
    // ORD-001, alice@example.com, SKU-A, 2, 49.90
    // ORD-001, alice@example.com, SKU-B, 1, 199.00
    std::vector<Order> parse(const std::string& csv) const {
        std::vector<Order> result;
        std::istringstream stream(csv);
        std::string line;

        std::getline(stream, line); // pula cabeçalho

        while (std::getline(stream, line)) {
            if (line.empty()) continue;

            std::istringstream row(line);
            std::string order_id, email, sku, qty_s, price_s;
            std::getline(row, order_id, ',');
            std::getline(row, email,    ',');
            std::getline(row, sku,      ',');
            std::getline(row, qty_s,    ',');
            std::getline(row, price_s,  ',');

            // trim espaços
            auto trim = [](std::string s) {
                size_t a = s.find_first_not_of(" \t");
                size_t b = s.find_last_not_of(" \t");
                return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
            };
            order_id = trim(order_id);
            email    = trim(email);
            sku      = trim(sku);

            OrderItem item{sku, std::stoi(trim(qty_s)),
                               std::stod(trim(price_s))};

            // Agrupa itens do mesmo pedido
            auto it = std::find_if(result.begin(), result.end(),
                [&](const Order& o){ return o.id == order_id; });

            if (it != result.end()) {
                it->items.push_back(item);
            } else {
                result.push_back({order_id, email, {item}});
            }
        }
        return result;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Etapa 2: OrderValidator
//  Razão única para mudar: mudança nas regras de validação de negócio.
// ─────────────────────────────────────────────────────────────────────────────
struct ValidationResult {
    bool        valid;
    std::string error;
};

class OrderValidator {
public:
    ValidationResult validate(const Order& order) const {
        if (order.id.empty())
            return {false, "order id cannot be empty"};

        if (order.customer_email.find('@') == std::string::npos)
            return {false, "invalid customer email: " + order.customer_email};

        if (order.items.empty())
            return {false, "order has no items"};

        for (const auto& item : order.items) {
            if (item.quantity <= 0)
                return {false, "invalid quantity for sku " + item.sku};
            if (item.unit_price <= 0.0)
                return {false, "invalid price for sku " + item.sku};
        }

        return {true, ""};
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Etapa 3: PricingEngine
//  Razão única para mudar: mudança nas alíquotas de imposto ou regras de preço.
// ─────────────────────────────────────────────────────────────────────────────
class PricingEngine {
public:
    explicit PricingEngine(double tax_rate = 0.15)
        : tax_rate_(tax_rate) {}

    double subtotal(const Order& order) const {
        double total = 0;
        for (const auto& item : order.items)
            total += item.unit_price * item.quantity;
        return total;
    }

    double tax(double subtotal) const {
        return subtotal * tax_rate_;
    }

private:
    double tax_rate_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Etapa 4: InvoiceBuilder
//  Razão única para mudar: mudança na estrutura ou formato da nota fiscal.
// ─────────────────────────────────────────────────────────────────────────────
class InvoiceBuilder {
public:
    explicit InvoiceBuilder(const PricingEngine& engine)
        : engine_(engine) {}

    Invoice build(const Order& order) const {
        double sub  = engine_.subtotal(order);
        double tax  = engine_.tax(sub);
        return Invoice{
            order.id,
            sub,
            tax,
            sub + tax,
            order.customer_email,
        };
    }

    std::string to_string(const Invoice& inv) const {
        return fmt::format(
            "  Invoice #{}\n"
            "    Customer : {}\n"
            "    Subtotal : R$ {:>8.2f}\n"
            "    Tax (15%%): R$ {:>8.2f}\n"
            "    Total    : R$ {:>8.2f}\n",
            inv.order_id,
            inv.customer_email,
            inv.subtotal,
            inv.tax,
            inv.total
        );
    }

private:
    const PricingEngine& engine_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Etapa 5: NotificationService
//  Razão única para mudar: mudança no canal ou conteúdo da notificação.
// ─────────────────────────────────────────────────────────────────────────────
class NotificationService {
public:
    void notify(const Invoice& inv) const {
        // Em produção: envia e-mail via SMTP ou fila de mensagens
        fmt::print("  [NotificationService] e-mail enviado para {}: "
                   "Pedido {} confirmado — Total R$ {:.2f}\n",
                   inv.customer_email, inv.order_id, inv.total);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Orquestrador: OrderProcessor
//  Responsabilidade única: compor e executar o pipeline.
//  Não implementa nenhuma das etapas — apenas as chama na ordem correta.
// ─────────────────────────────────────────────────────────────────────────────
class OrderProcessor {
public:
    OrderProcessor(const CsvParser&        parser,
                   const OrderValidator&   validator,
                   const InvoiceBuilder&   builder,
                   const NotificationService& notifier)
        : parser_(parser)
        , validator_(validator)
        , builder_(builder)
        , notifier_(notifier)
    {}

    void process(const std::string& csv) const {
        auto orders = parser_.parse(csv);

        for (const auto& order : orders) {
            auto [valid, error] = validator_.validate(order);
            if (!valid) {
                fmt::print("  [SKIP] pedido {} inválido: {}\n",
                           order.id, error);
                continue;
            }

            auto invoice = builder_.build(order);
            fmt::print("{}", builder_.to_string(invoice));
            notifier_.notify(invoice);
        }
    }

private:
    const CsvParser&          parser_;
    const OrderValidator&     validator_;
    const InvoiceBuilder&     builder_;
    const NotificationService& notifier_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Demo
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 2: Pipeline SRP — processamento de pedidos ───────┐\n");

    const std::string csv = R"(order_id,customer_email,sku,qty,price
ORD-001, alice@example.com, SKU-A, 2, 49.90
ORD-001, alice@example.com, SKU-B, 1, 199.00
ORD-002, bob@example.com,   SKU-C, 3, 29.99
ORD-003, sem-arroba-invalido, SKU-D, 1, 10.00
)";

    // Cada colaborador é criado independentemente e injetado no orquestrador
    CsvParser          parser;
    OrderValidator     validator;
    PricingEngine      pricing{0.15};
    InvoiceBuilder     builder{pricing};
    NotificationService notifier;

    OrderProcessor processor{parser, validator, builder, notifier};
    processor.process(csv);

    fmt::print("\n");
    fmt::print("  Cada classe mudaria por exatamente uma razão:\n");
    fmt::print("    CsvParser          ← mudança no formato de entrada\n");
    fmt::print("    OrderValidator     ← mudança nas regras de validação\n");
    fmt::print("    PricingEngine      ← mudança nas alíquotas\n");
    fmt::print("    InvoiceBuilder     ← mudança na estrutura da nota\n");
    fmt::print("    NotificationService← mudança no canal de notificação\n");
    fmt::print("    OrderProcessor     ← mudança na orquestração do fluxo\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_pipeline
