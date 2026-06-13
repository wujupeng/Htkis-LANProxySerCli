<template>
  <div>
    <el-row style="margin-bottom:12px" :gutter="16">
      <el-col :span="6">
        <el-select v-model="levelFilter" @change="clearLogs">
          <el-option value="" :label="$t('logs.all')" />
          <el-option value="info" :label="$t('logs.info')" />
          <el-option value="warn" :label="$t('logs.warn')" />
          <el-option value="error" :label="$t('logs.error')" />
        </el-select>
      </el-col>
      <el-col :span="4">
        <el-button @click="autoScroll = !autoScroll">
          {{ autoScroll ? $t('logs.pause') : $t('logs.autoScroll') }}
        </el-button>
      </el-col>
    </el-row>

    <el-table :data="logs" size="small" max-height="600" ref="tableRef">
      <el-table-column prop="ts" label="Time" width="180">
        <template #default="{ row }">{{ formatTime(row.ts) }}</template>
      </el-table-column>
      <el-table-column prop="level" label="Level" width="80">
        <template #default="{ row }">
          <el-tag :type="row.level === 'error' ? 'danger' : row.level === 'warn' ? 'warning' : 'info'" size="small">
            {{ row.level }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column prop="module" label="Module" width="120" />
      <el-table-column prop="msg" label="Message" />
    </el-table>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, nextTick } from 'vue'
import request from '../utils/request'

const logs = ref<any[]>([])
const levelFilter = ref('')
const autoScroll = ref(true)
const tableRef = ref()
let ws: WebSocket | null = null
let timer: any

function formatTime(ts: number) { return new Date(ts).toLocaleString() }
function clearLogs() { logs.value = [] }

function connectWS() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:'
  ws = new WebSocket(`${proto}//${location.host}/ws/logs`)
  ws.onmessage = (event) => {
    try {
      const entry = JSON.parse(event.data)
      if (!levelFilter.value || entry.level === levelFilter.value) {
        logs.value.push(entry)
        if (logs.value.length > 2000) logs.value.shift()
        if (autoScroll.value) {
          nextTick(() => {
            const el = tableRef.value?.$el?.querySelector('.el-scrollbar__wrap')
            if (el) el.scrollTop = el.scrollHeight
          })
        }
      }
    } catch {}
  }
  ws.onclose = () => { setTimeout(connectWS, 3000) }
}

async function fetchLogs() {
  try {
    const { data } = await request.get('/api/logs?count=100')
    logs.value = Array.isArray(data) ? data : []
  } catch {}
}

onMounted(() => {
  fetchLogs()
  connectWS()
  timer = setInterval(fetchLogs, 30000)
})
onUnmounted(() => {
  ws?.close()
  clearInterval(timer)
})
</script>
