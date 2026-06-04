# SPDX-FileCopyrightText: 2023 Ledger SAS
# SPDX-License-Identifier: Apache-2.0

gpdma_h = custom_target('gen_gpdma',
    input: peripheral_defs_in,
    output: '@0@_defs.h'.format('gpdma'),
    depends: [ svd_json ],
    command: [
        jinja_cli, '-d', svd_json, '-o', '@OUTPUT@', '--define', 'NAME',
       'GPDMA',
       '@INPUT@'
    ],
)

bsp_private_gen_header_set.add(gpdma_h)

stm32_gpdma_dts_template_c = files(['stm32-gpdma-dt.c.in'])
stm32_gpdma_dts_template_h = files(['stm32-gpdma-dt.h.in'])

stm32_gpdma_dtsgen_c = dtsgen.process(stm32_gpdma_dts_template_c)
stm32_gpdma_dtsgen_h = dtsgen.process(stm32_gpdma_dts_template_h)


bsp_private_gen_source_set.add(when: 'CONFIG_SOC_SUBFAMILY_STM32U5', if_true: [ stm32_gpdma_dtsgen_c, stm32_gpdma_dtsgen_h ])

bsp_private_gen_source_set.add(when: 'CONFIG_SOC_SUBFAMILY_STM32U5', if_true: files('stm32u5-gpdma.c'))

# driver source selection

if kconfig_data.get('CONFIG_SOC_SUBFAMILY_RP2350', 0) == 1
subdir('rp2350')
endif
