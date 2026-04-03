#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <cmath>
#include <string>
#include <vector>

// ============================================================
// CONCEITO CENTRAL DO DEAR IMGUI
//
// O loop principal faz sempre a mesma sequência:
//
//   1. Poll events         — teclado, mouse, resize
//   2. NewFrame            — ImGui começa a coletar widgets
//   3. [sua UI aqui]       — você descreve o que quer ver
//   4. Render              — ImGui gera os vértices
//   5. swap buffers        — OpenGL apresenta o frame
//
// Não há objetos de widget persistentes. Tudo que você não
// chamar neste frame simplesmente não aparece.
// ============================================================

// ── estado da aplicação ─────────────────────────────────────
// No immediate mode, o estado fica no seu código, não na UI.
// ImGui lê e escreve nessas variáveis diretamente via ponteiros.
struct AppState {
    // sliders
    float frequency  = 1.0f;
    float amplitude  = 1.0f;
    float phase      = 0.0f;

    // inputs de texto
    char  name[128]  = "Lucas";
    int   iterations = 10;

    // botões / toggles
    bool  show_sine  = true;
    bool  show_cos   = false;
    bool  dark_mode  = true;

    // combo
    int   wave_type  = 0;

    // histórico para o plot
    std::vector<float> history;
    float              time = 0.0f;
};

// ============================================================
// JANELA: controles
// ============================================================
static void draw_controls(AppState& s) {
    ImGui::Begin("Controles");

    // ── seção: onda ─────────────────────────────────────────
    if (ImGui::CollapsingHeader("Onda", ImGuiTreeNodeFlags_DefaultOpen)) {

        // SliderFloat: label, ponteiro para valor, min, max
        ImGui::SliderFloat("Frequência", &s.frequency, 0.1f, 5.0f);
        ImGui::SliderFloat("Amplitude",  &s.amplitude, 0.1f, 2.0f);
        ImGui::SliderFloat("Fase",       &s.phase,     0.0f, 6.28f);

        // combo box
        const char* waves[] = {"Senoide", "Cosseno", "Quadrada"};
        ImGui::Combo("Tipo", &s.wave_type, waves, 3);

        // checkboxes
        ImGui::Checkbox("Mostrar seno",     &s.show_sine);
        ImGui::SameLine();  // próximo widget na mesma linha
        ImGui::Checkbox("Mostrar cosseno",  &s.show_cos);
    }

    ImGui::Spacing();

    // ── seção: inputs ────────────────────────────────────────
    if (ImGui::CollapsingHeader("Inputs")) {

        // input de texto
        ImGui::InputText("Nome", s.name, sizeof(s.name));

        // input inteiro com setas
        ImGui::InputInt("Iterações", &s.iterations);
        if (s.iterations < 1)  s.iterations = 1;
        if (s.iterations > 100) s.iterations = 100;

        // botão normal
        if (ImGui::Button("Resetar")) {
            s.frequency  = 1.0f;
            s.amplitude  = 1.0f;
            s.phase      = 0.0f;
            s.iterations = 10;
        }

        ImGui::SameLine();

        // botão colorido
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Aplicar")) {
            fmt::println("aplicando: nome={} iter={}", s.name, s.iterations);
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // ── seção: aparência ─────────────────────────────────────
    if (ImGui::CollapsingHeader("Aparência")) {
        if (ImGui::Checkbox("Modo escuro", &s.dark_mode)) {
            if (s.dark_mode) ImGui::StyleColorsDark();
            else             ImGui::StyleColorsLight();
        }
    }

    // ── rodapé com info ──────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("%.1f fps", ImGui::GetIO().Framerate);

    ImGui::End();
}

// ============================================================
// JANELA: visualização da onda
// ============================================================
static void draw_plot(AppState& s) {
    ImGui::Begin("Visualização");

    // avança o tempo e calcula o valor atual
    s.time += ImGui::GetIO().DeltaTime;

    float value = 0.0f;
    float arg   = s.frequency * s.time + s.phase;

    if (s.wave_type == 0)       value = s.amplitude * sinf(arg);
    else if (s.wave_type == 1)  value = s.amplitude * cosf(arg);
    else {                      // onda quadrada
        value = s.amplitude * (sinf(arg) >= 0.0f ? 1.0f : -1.0f);
    }

    // mantém histórico dos últimos 200 pontos
    s.history.push_back(value);
    if (s.history.size() > 200)
        s.history.erase(s.history.begin());

    // PlotLines: gráfico de linha simples — sem biblioteca externa
    ImGui::PlotLines("##onda",
        s.history.data(),
        static_cast<int>(s.history.size()),
        0,
        nullptr,
        -2.5f, 2.5f,          // range Y
        ImVec2(0, 120));       // tamanho (0 = largura automática)

    // valor atual como barra de progresso
    float normalized = (value / s.amplitude + 1.0f) / 2.0f;  // [0, 1]
    ImGui::ProgressBar(normalized, ImVec2(-1, 0), "");

    ImGui::Spacing();
    ImGui::Text("valor atual: %.4f", value);
    ImGui::Text("tempo:       %.2f s", s.time);

    ImGui::End();
}

// ============================================================
// JANELA: tabela de valores calculados
// ============================================================
static void draw_table(AppState& s) {
    ImGui::Begin("Tabela");

    if (ImGui::BeginTable("valores", 3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

        ImGui::TableSetupColumn("i");
        ImGui::TableSetupColumn("x");
        ImGui::TableSetupColumn("sin(f·x + φ)");
        ImGui::TableHeadersRow();

        for (int i = 0; i < s.iterations; ++i) {
            float x   = static_cast<float>(i) / s.iterations * 6.28f;
            float val = s.amplitude * sinf(s.frequency * x + s.phase);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", x);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", val);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

int main() {
    // ── inicializa GLFW ──────────────────────────────────────
    if (!glfwInit()) {
        fmt::println("erro: glfwInit falhou");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Academy — ImGui", nullptr, nullptr);
    if (!window) {
        fmt::println("erro: glfwCreateWindow falhou");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    // ── inicializa ImGui ─────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // janelas encaixáveis

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ── estado da aplicação ──────────────────────────────────
    AppState state;

    // ── loop principal ───────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // começa novo frame ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // dockspace — permite encaixar janelas
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // desenha as três janelas
        draw_controls(state);
        draw_plot(state);
        draw_table(state);

        // renderiza
        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // ── cleanup ──────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}