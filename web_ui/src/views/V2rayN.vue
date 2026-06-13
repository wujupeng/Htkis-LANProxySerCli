<template>
  <div>
    <el-tabs v-model="activeTab">
      <el-tab-pane label="状态控制" name="status">
        <el-card style="margin-bottom:16px">
          <template #header>{{ $t('v2rayn.status') }}</template>
          <el-row :gutter="16" align="middle">
            <el-col :span="8">
              <el-tag :type="status.status === 'running' ? 'success' : status.status === 'failed' ? 'danger' : 'info'" size="large">
                {{ status.status }}
              </el-tag>
              <span style="margin-left:12px">PID: {{ status.pid || '-' }}</span>
              <span style="margin-left:12px">Crashes: {{ status.crash_count || 0 }}</span>
            </el-col>
            <el-col :span="8">
              <el-button @click="startV2rayN" :disabled="status.status === 'running'">{{ $t('v2rayn.start') }}</el-button>
              <el-button @click="stopV2rayN" :disabled="status.status !== 'running'" type="danger">{{ $t('v2rayn.stop') }}</el-button>
              <el-button @click="restartV2rayN" type="warning">{{ $t('v2rayn.restart') }}</el-button>
            </el-col>
          </el-row>
        </el-card>
      </el-tab-pane>
      <el-tab-pane label="VMess节点管理" name="vmess">
        <vm-node-list />
      </el-tab-pane>
      <el-tab-pane label="JSON配置" name="json">
        <el-card>
          <template #header>{{ $t('v2rayn.config') }}</template>
          <el-input v-model="configText" type="textarea" :rows="20" style="font-family:monospace" />
          <div style="margin-top:12px">
            <el-button type="primary" @click="applyConfig">{{ $t('v2rayn.apply') }}</el-button>
          </div>
        </el-card>
      </el-tab-pane>
    </el-tabs>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
import request from '../utils/request'
import VmNodeList from './vmess/VmNodeList.vue'

const activeTab = ref('status')
const status = ref<any>({})
const configText = ref('{}')
let timer: any

async function fetchStatus() {
  const { data } = await request.get('/api/v2rayn/status')
  status.value = data
}

async function fetchConfig() {
  const { data } = await request.get('/api/v2rayn/config')
  configText.value = typeof data === 'string' ? data : JSON.stringify(data, null, 2)
}

async function startV2rayN() { await request.post('/api/v2rayn/start'); fetchStatus() }
async function stopV2rayN() { await request.post('/api/v2rayn/stop'); fetchStatus() }
async function restartV2rayN() { await request.post('/api/v2rayn/restart'); fetchStatus() }

async function applyConfig() {
  try {
    const config = JSON.parse(configText.value)
    await request.put('/api/v2rayn/config', config)
    ElMessage.success('OK')
  } catch (e) {
    ElMessage.error('Invalid JSON')
  }
}

onMounted(() => {
  fetchStatus()
  fetchConfig()
  timer = setInterval(fetchStatus, 5000)
})
onUnmounted(() => clearInterval(timer))
</script>
