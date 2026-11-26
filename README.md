# Xclipse GPU Lab 0.3 alpha
Laboratório Android ARM64 de diagnóstico de GPU. Não contém llama.cpp, modelos de IA, internet, nem treinamento. Projetado para Xclipse 940 / Exynos 2400(e), mas as consultas públicas podem funcionar em outras GPUs.

## Funcionalidades implementadas no código
- Vulkan: versão, driver, IDs, extensões de instância/dispositivo, layers, filas, limites selecionados, features selecionadas, formatos core, memória e orçamento (quando anunciado), subgroup, FP16/INT8, dot product acelerado, mesh, ray tracing, cooperative matrix (formas/tipos), nomes e metadados de contadores de performance. As estruturas de extensão são consultadas apenas quando anunciadas ou promovidas ao core aplicável.
- EGL / GLES: contexto pbuffer próprio, renderer, versões, extensões e limites selecionados.
- OpenCL: tenta carregar a biblioteca pública libOpenCL.so, enumera plataformas e dispositivos, extensões e compute units. O Android pode bloquear a biblioteca por namespace. Ausência de acesso não prova ausência física.
- Kernel/SGPU: propriedades, nós devfreq, DRM, dumpsys gpu, arquivos de diagnóstico e térmicos GPU; root opcional, apenas se o dispositivo já o oferece. Sem exploit, sem mudança de permissões, sem escrita em sysfs/debugfs, sem desativar SELinux. Sensores e unidades são específicos da ROM; valores brutos são mostrados sem inventar porcentagens/Hz/temperaturas. O monitor reconsulta aproximadamente a cada 2 s após a leitura, apenas com a tela ativa.
- Bibliotecas ELF: lista arquivos acessíveis em diretórios gráficos, classe/arquitetura ELF, SHA-256, busca de strings VK/GL/EGL/CL, gfx11, WMMA e wave. Não descompila código, não lê áreas protegidas por root e não prova que as strings sejam símbolos exportados. Faça a consulta Vulkan antes da análise ELF para comparar strings VK_ não anunciadas; strings podem vir do loader, tabelas genéricas ou caminhos mortos. Limite 6000 pistas por arquivo.
- Investigação de extensão: processo de app separado (:probe), não isolado por UID. Teste de criação de dispositivo para quatro extensões selecionadas e anunciadas em Vulkan >=1.2; outras são apenas classificadas. Não habilita feature bits nem executa comandos específicos; dependências complexas ficam pendentes. Sem substituição de biblioteca ou configuração global. Uma falha de driver ainda pode afetar o sistema; não é uma sandbox do kernel.
- Exportação TXT via seletor Android e cópia do último relatório; compare manualmente relatórios de ROMs. Nenhum upload automático.

## Significado dos resultados
1. ANUNCIADA: o driver/loader declara a extensão.
2. ACEITA: vkCreateDevice aceitou a extensão na configuração testada; não comprova correção funcional.
3. INDÍCIO ELF: string presente, sem comprovação de código ou silício.
4. NÃO ANUNCIADA: não exposta pela API consultada. Não significa que seja possível ativá-la nem que esteja fisicamente ausente.
5. INDISPONÍVEL: consulta não acessível, não implementada, ou bloqueada pelo sistema.

A especificação exige que o driver rejeite extensões indisponíveis. Root não é uma chave universal de desbloqueio. A v0.1 não força extensões ocultas, não modifica o driver proprietário, não instala RADV, não captura registradores MMIO e não executa microbenchmarks de shaders/ISA. Contadores são enumerados, não amostrados via performance queries. A análise de driver alternativo e microtestes com resultados conhecidos são próximos passos.

## Compilação
- Android SDK 35, NDK 27.2.12479018, CMake 3.22.1, JDK 17, Gradle 8.9; headers Vulkan v1.4.321.
- No GitHub, crie um repositório com os arquivos deste ZIP e execute Actions → Android ARM64 APK → Run workflow. O workflow baixa headers e ferramentas e publica o APK debug como artefato. Nenhum APK é incluído neste ZIP.
- Localmente, defina VULKAN_HEADERS para o diretório include dos headers Vulkan v1.4.321 e execute `gradle :app:assembleDebug` com SDK/NDK instalados.

## Validação e limitações
O ambiente de criação não tem Android SDK/NDK, Gradle ou headers Vulkan. APK e código JNI não foram compilados/executados aqui. Os módulos Java independentes Kernel/Libraries foram compilados com javac; comparação sintética de strings e sintaxe do shell foram verificadas. O funcionamento no S24 FE, acesso ao root, estabilidade e disponibilidade de sensores dependem de testes reais. O APK deverá ser tratado como alpha.

Referências públicas: https://developer.android.com/ndk/guides/graphics/extensions ; https://developer.android.com/ndk/guides/graphics/validation-layer ; https://source.android.com/docs/core/graphics/implement-vulkan

## Atualização 0.2 — auditoria de capacidades
- Geração no build de consultas independentes a estruturas `VkPhysicalDeviceFeatures2` e `VkPhysicalDeviceProperties2` a partir do vk.xml dos Vulkan-Headers fixados. Consulta somente estruturas elegíveis por versão core ou extensão anunciada; exclui extensões de plataforma/provisórias, requisitos condicionais e estruturas com ponteiros de saída (exceto pNext). Campos escalares/arrays são impressos; agregados não decodificados são explicitados. Não é cobertura integral de todos os comandos/formatos/extensions Vulkan.
- Classificação por GPU e estrutura: bits consultados ativos, features consultadas desativadas, parcial (bits opcionais), inconsistente (VkBool32 inválido), anunciada sem auditoria de feature. A extensão pode ser plenamente conforme mesmo quando há bits opcionais false. Não prova capacidade física ou correção de shaders. Continua a lista de candidatas não anunciadas e indícios de strings ELF do v0.1.
- Caminhos de bibliotecas gráficas carregadas no processo da consulta Vulkan; disponibilidade de fontes de tracing e contabilidade de memória do kernel. Nenhuma ativação de trace, escrita em kernel ou alteração de driver. Root opcional permanece limitado a leitura de diagnósticos selecionados.
- Compilação requer Python 3 e checkout completo dos Vulkan-Headers (não só include). Workflow gera a auditoria a partir do registro oficial. Teste Java sintético e fixture XML do gerador passaram localmente. O gerador não foi executado contra o registro oficial aqui e o APK/JNI continuam não compilados por falta do SDK/NDK e do registro oficial no ambiente. A CI foi configurada, mas não executada.
- Ainda pendentes: testes funcionais de shaders, análise de símbolos ELF real (em vez de strings), captura/amostragem de contadores, consultas completas de vídeo/formatos estendidos, comparação automatizada entre drivers, análise de ISA/firmware. Não há desbloqueio de extensões ocultas. Recursos inacessíveis devem ser tratados como inconclusivos, não como ausência do silício.

## Atualização 0.3 — pesquisa de arquitetura
- Anti-Lag e Vulkan Video removidos dos candidatos prioritários. A enumeração automática continua fiel ao driver; Mesh Shader permanece somente para comparação com o RADV (relato anterior do usuário, sem relatório anexado).
- Inventário recursivo com lista de caminhos 32/64 bits das bibliotecas nas partições vendor, odm, system, system_ext, product e APEX acessíveis ao app. Parser ELF de seção com .dynsym, .symtab, dependências DT_NEEDED e SONAME, exports/imports e símbolos relevantes; segue dependências por nome. Limites de segurança: profundidade 16, 20000 arquivos, 350 bibliotecas candidatas, 6000 símbolos por classe; bibliotecas sem section headers ou protegidas podem ser inconclusivas. Não é descompilação, não resolve namespaces do linker, não usa root para ler arquivos protegidos.
- Inventário e SHA-256 de candidatos a firmware de GPU acessíveis; não carrega, altera ou descompila firmware. O nome do arquivo não confirma que está em uso.
- Silicon Explorer: mapa lógico clicável e perfil por bloco com valores anunciados, bits ativos/desativados, suporte parcial, indícios ELF, resultados de compute; não representa uma fotografia ou planta física do silício. Catálogo inicial de candidatos RDNA 3, não uma base completa da ISA. Fonte de referência: https://gpuopen.com/amd-gpu-architecture-programming-documentation/ .
- Comparação de relatórios Vulkan salvos/importados para Samsung vs RADV (ou ROMs distintas). Não instala ou seleciona outro driver automaticamente.
- Testes compute SPIR-V em processo separado: UINT32, subgroup arithmetic, FP16, INT8 e tamanho de subgroup requerido 32/64 quando anunciado. Um workgroup de 64 invocações, leitura e verificação contra resultados esperados; PASS/FAIL/SKIP/INCONCLUSIVE. Timeout de fence 5 s encerra o processo de teste; travamento no driver ainda pode afetar o sistema. Não força recursos desanunciados. A precisão de uma operação de shader não confirma a instrução ISA usada, nem suporte WMMA. Tempos são CPU submit-to-fence, não timestamps físicos. Testes requerem Vulkan 1.2 e memória host coherent.
- CI: compila SPIR-V com glslc, testa parser/classificação e gera APK. O build anterior da v0.2 foi bem-sucedido no GitHub; a v0.3 precisa de novo build e execução real.
- Pendentes: microtestes cooperative matrix/WMMA, validação de ISA específica via desassemblagem de shader, amostragem de contadores físicos, análise de firmware além de metadados, investigação da extensão privada Samsung, análise de bibliotecas sem seções, evidência de layout real e testes de extensões não anunciadas via driver alternativo. Nenhuma mudança no kernel, firmware, SELinux ou driver original.
