/*****************************************************************************
 *
 *    Copyright (c) 2016-2026 by Sophgo Technologies Inc. All rights reserved.
 *
 *    The material in this file is confidential and contains trade secrets
 *    of Sophgo Technologies Inc. This is proprietary information owned by
 *    Sophgo Technologies Inc. No part of this work may be disclosed,
 *    reproduced, copied, transmitted, or used in any way for any purpose,
 *    without the express written permission of Sophgo Technologies Inc.
 *
 *****************************************************************************/

/*****************************************************************************
 * Bmodel Interface is mainly for inference.
 ****************************************************************************/

#ifndef BMODEL_INTERFACE_H_
#define BMODEL_INTERFACE_H_

#ifdef _WIN32
#ifndef DECL_EXPORT
#define DECL_EXPORT _declspec(dllexport)
#endif
#ifndef DECL_IMPORT
#define DECL_IMPORT _declspec(dllimport)
#endif
#else
#define DECL_EXPORT
#define DECL_IMPORT
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef void* tpum_handel_t;

/* --------------------------------------------------------------------------*/
/**
 * @name    tpu_model_create
 * @brief   To create the bmodel.
 * @ingroup bmodel
 *
 * This API creates the bmodel. It returns a void* pointer which is the pointer
 * of bmodel.
 *
 * @retval tpum_handel_t the pointer of bmodel
 */
DECL_EXPORT tpum_handel_t tpu_model_create();

/* --------------------------------------------------------------------------*/
/**
 * @name    tpu_model_add
 * @brief   To add bmodel to p_bmodel
 * @ingroup bmodel
 *
 * This API loads bmodel to p_bmodel.
 *
 * @param [in] p_bmodel       Bmodel that had been created
 * @param [in] bmodel_data   The first bmodel data
 * @param [in] size          The first bmodel size
 *
 * @retval void
 */
DECL_EXPORT void tpu_model_add(
    tpum_handel_t p_bmodel, const void* bmodel_data, unsigned long long size);

/* --------------------------------------------------------------------------*/
/**
 * @name    tpu_model_combine
 * @brief   To combine bmodels to one.
 * @ingroup bmodel
 *
 * This API combines all bmodels to one. Models are load by bmd_load_bmodel.
 * Once bmodel has been combined, do not call this interface again.
 *
 * @param [in] p_bmodel       Bmodel that had been created
 *
 * @retval unsigned long long the size of combined bmodel
 */
DECL_EXPORT unsigned long long tpu_model_combine(tpum_handel_t p_bmodel);

/* --------------------------------------------------------------------------*/
/**
 * @name    tpu_model_save
 * @brief   To save bmodel data to buffer.
 * @ingroup bmodel
 *
 * This API save combined bmodel to bmodel_data
 *
 * @param [in] p_bmodel       Bmodel that had been created
 * @param [in] bmodel_data    bmodel data pointer
 *
 * @retval void
 */
DECL_EXPORT void tpu_model_save(tpum_handel_t p_bmodel, void* bmodel_data);

/* --------------------------------------------------------------------------*/
/**
 * @name    tpu_model_destroy
 * @brief   To destroy the bmodel pointer
 * @ingroup bmodel
 *
 * This API destroy the bmodel.
 *
 * @param [in] p_bmodel       Bmodel that had been created
 *
 * @retval void
 */
DECL_EXPORT void tpu_model_destroy(tpum_handel_t p_bmodel);

#if defined (__cplusplus)
}
#endif

#endif
