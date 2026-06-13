<template>
  <el-card>
    <template #header>{{ $t('settings.title') }}</template>
    <el-form :model="form" label-width="180px" style="max-width:600px">
      <el-form-item :label="$t('settings.proxyPort')">
        <el-input-number v-model="form.proxy_port" :min="1" :max="65535" />
      </el-form-item>
      <el-form-item :label="$t('settings.webPort')">
        <el-input-number v-model="form.web_ui_port" :min="1" :max="65535" />
      </el-form-item>
      <el-form-item :label="$t('settings.threadCount')">
        <el-input-number v-model="form.proxy_thread_count" :min="1" :max="64" />
      </el-form-item>
      <el-form-item :label="$t('settings.defaultRoute')">
        <el-select v-model="form.default_route_action">
          <el-option value="direct" :label="$t('rules.direct')" />
          <el-option value="proxy" :label="$t('rules.proxy')" />
        </el-select>
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="saveSettings">{{ $t('common.save') }}</el-button>
      </el-form-item>
    </el-form>
  </el-card>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import request from '../utils/request'

const form = ref<any>({})

async function fetchSettings() {
  const { data } = await request.get('/api/settings')
  form.value = data
}

async function saveSettings() {
  const { data } = await request.put('/api/settings', form.value)
  if (data.need_restart) ElMessage.warning('Need restart to take effect')
  else ElMessage.success('OK')
}

onMounted(fetchSettings)
</script>
