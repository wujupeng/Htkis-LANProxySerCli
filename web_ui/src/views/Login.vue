<template>
  <div style="display:flex;justify-content:center;align-items:center;height:100vh;background:#f0f2f5">
    <el-card style="width:400px">
      <h2 style="text-align:center;margin-bottom:24px">{{ $t('login.title') }}</h2>
      <el-form :model="form" @submit.prevent="handleLogin">
        <el-form-item :label="$t('login.username')">
          <el-input v-model="form.username" />
        </el-form-item>
        <el-form-item :label="$t('login.password')">
          <el-input v-model="form.password" type="password" show-password />
        </el-form-item>
        <el-button type="primary" style="width:100%" @click="handleLogin"
                   :loading="loading">{{ $t('login.submit') }}</el-button>
      </el-form>
    </el-card>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive } from 'vue'
import { useI18n } from 'vue-i18n'
import { ElMessage } from 'element-plus'
import router from '../router'
import request from '../utils/request'

const { t } = useI18n()
const loading = ref(false)
const form = reactive({ username: '', password: '' })

async function handleLogin() {
  loading.value = true
  try {
    const { data } = await request.post('/api/auth/login', form)
    localStorage.setItem('token', data.token)
    ElMessage.success(t('common.ok'))
    router.push('/dashboard')
  } catch (e: any) {
    if (e.response?.status === 429) ElMessage.error(t('login.locked'))
  } finally {
    loading.value = false
  }
}
</script>
